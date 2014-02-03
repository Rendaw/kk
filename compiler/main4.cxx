#include "type.h"
#include "extrastandard.h"

#include <string>
#include <functional>
#include <map>
#include <iostream>
#include <cassert>
#include <memory>
#include <json/json.h>
#include <json/json_tokener.h>
#include <json/json_object.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/IR/IRBuilder.h>
//#include <llvm/ModuleProvider.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

/* NOTS
Typo: missed e

Pass by copy.  Values only changed through assignment by default.  Clone doesn't clone reference arguments cuz like they're references.  Make sure to clone before any operations that could modify stuff.

Records can be rearranged freely when passed to functions.  Error if arrangement wrong on a struct?  Yes.  Struct inconsistencies may cause unnecessary performance penalties but only the user knows that.

*/

// Fucking standard
struct ShareableT
{
	virtual ~ShareableT(void) { assert(RefCount == 0); }
	private:
		friend void IncrementSharedPointer(ShareableT *Shareable);
		friend void DecrementSharedPointer(ShareableT *Shareable);
		friend size_t CountSharedPointer(ShareableT *Shareable);
		size_t RefCount = 0;
};

void IncrementSharedPointer(ShareableT *Shareable) 
{ 
	if (!Shareable) return; 
	++Shareable->RefCount; 
}

void DecrementSharedPointer(ShareableT *Shareable) 
{ 
	if (!Shareable) return; 
	--Shareable->RefCount; 
	assert(Shareable->RefCount >= 0); 
	if (Shareable->RefCount == 0) delete Shareable; 
}

size_t CountSharedPointer(ShareableT *Shareable) 
	{ return Shareable->RefCount; }

template <typename BaseT> struct SharedPointerT
{
	SharedPointerT(void) : Base(nullptr) {}
	SharedPointerT(BaseT *Base) : Base(Base) { Set(); }
	SharedPointerT(SharedPointerT<BaseT> &Base) : Base(Base) { Set(); }
	SharedPointerT(SharedPointerT<BaseT> &&Base) : Base(Base) { Set(); }
	~SharedPointerT(void) { Clear(); }
	SharedPointerT<BaseT> &operator =(BaseT *Base) { Clear(); this->Base = Base; Set(); return *this; }
	SharedPointerT<BaseT> &operator =(SharedPointerT<BaseT> &Base) { Clear(); this->Base = Base; Set(); return *this; }
	SharedPointerT<BaseT> &operator =(SharedPointerT<BaseT> &&Base) { Clear(); this->Base = Base; Set(); return *this; }

	BaseT *operator ->(void) { return Base; }
	BaseT const *operator ->(void) const { return Base; }
	BaseT &operator *(void) { return *Base; }
	BaseT const &operator *(void) const { return *Base; }
	operator BaseT *(void) { return Base; }
	operator BaseT const *(void) const { return Base; }
	bool operator !(void) const { return Base; }
	bool operator ==(BaseT const *Other) const { return Base == Other; }
	bool operator ==(SharedPointerT<BaseT> const &Other) const { return Base == Other.Base; }
	bool operator !=(BaseT const *Other) const { return Base != Other; }

	size_t Count(void) const { return CountSharedPointer(Base); }

	private:
		void Set(void) { IncrementSharedPointer(Base); }
		void Clear(void) { DecrementSharedPointer(Base); Base = nullptr; }
		BaseT *Base;
};

///////////////////////////////////////////////////////
inline llvm::Type *LLVMInt(unsigned int Size) { return llvm::IntegerType::get(llvm::getGlobalContext(), Size); }

struct SingleT;
struct AtomT : ShareableT
{
	AtomT *Context = nullptr;
  
	std::list<SingleT *> References;
	void Replace(AtomT *Other);
	
	virtual AtomT *Type(void) { return nullptr; }
	virtual AtomT *Clone(void) = 0;
	virtual void Simplify(void) {}
};

struct SingleT
{
	private:
		mutable SharedPointerT<AtomT> Internal; // mutable because the standard library is insane
		void Clean(void) 
		{ 
			if (Internal) 
				Internal->References.erase(
					std::find(
						Internal->References.begin(), 
						Internal->References.end(), 
						this)); 
		}
	public:

	SingleT(void) {}
	SingleT(AtomT *Base) : Internal(Base) 
		{ if (Internal) Internal->References.push_back(this); }
	SingleT(SingleT &Base) : Internal(Base.Internal) 
		{ if (Internal) Internal->References.push_back(this); }
	SingleT(SingleT const &Base) : Internal(Base.Internal) 
		{ if (Internal) Internal->References.push_back(this); }
	SingleT(SingleT &&Base) : Internal(Base.Internal) 
		{ if (Internal) Internal->References.push_back(this); }

	~SingleT(void) { Clean(); }

	SingleT &operator =(AtomT *Base) 
	{ 
		Clean(); 
		Internal = Base; 
		if (Internal) Internal->References.push_back(this); 
		return *this; 
	}

	SingleT &operator =(SingleT &Base) 
	{ 
		Clean(); 
		Internal = Base; 
		if (Internal) Internal->References.push_back(this); 
		return *this; 
	}
	
	SingleT &operator =(SingleT &&Base) 
	{ 
		Clean(); 
		Internal = Base; 
		if (Internal) Internal->References.push_back(this); 
		return *this; 
	}

	AtomT *operator ->(void) { return Internal; }
	AtomT const *operator ->(void) const { return Internal; }
	AtomT *operator *(void) { return Internal; }
	AtomT const *operator *(void) const { return Internal; }
	operator AtomT *(void) { return Internal; }
	operator AtomT const *(void) const { return Internal; }
	operator bool(void) const { return static_cast<bool>(Internal); }

	bool operator == (SingleT const &Other) const { return Internal == Other.Internal; }
	bool operator < (SingleT const &Other) const { return Internal < Other.Internal; }
};
	
void AtomT::Replace(AtomT *Other)
{
	SharedPointerT<AtomT> Keep(this);
	auto References = this->References;
	for (auto &Reference : References)
		if (Reference) *Reference = Other;
	References.clear();
	assert(Keep.Count() == 1);
}

typedef std::list<SingleT> MultipleT;

/*struct UndefinedT : AtomT 
{ 
	AtomT *Clone(void) { return new UndefinedT; } 
};

enum struct ExistenceT { ConstantE, StaticE, DynamicE };
bool IsConstant(ExistenceT const &Value) { return Value == ExistenceT::ConstantE; }
bool IsDynamic(ExistenceT const &Value) { return (Value == ExistenceT::StaticE) || (Value == ExistenceT::DynamicE); }

struct TypeBaseT : AtomT
{
	ExistenceT Existence;
	virtual llvm::Type *GenerateType(void) = 0;

	bool operator == (TypeBaseT const &Other) const { return typeid(*this) == typeid(Other); }
	bool operator != (TypeBaseT const &Other) const { return !(*this == Other); }
	
	TypeBaseT(ExistenceT const &Existence = ExistenceT::ConstantE) : Existence(Existence) {}
};

struct IntTypeT : TypeBaseT
{
	using TypeBaseT::TypeBaseT;
	TypeBaseT *Clone(void) { return new IntTypeT{Existence}; }
	llvm::Type *GenerateType(void) { return LLVMInt(32); }
};

struct StringTypeT : TypeBaseT
{
	using TypeBaseT::TypeBaseT;
	TypeBaseT *Clone(void) { return new StringTypeT{Existence}; }
	llvm::Type *GenerateType(void) { return llvm::PointerType::getUnqual(LLVMInt(8)); } 
};

struct ExpressionBaseT : AtomT
{
	virtual llvm::Value *GenerateLoad(llvm::BasicBlock *Block) = 0;
};

struct DynamicT : ExpressionBaseT
{
	SingleT InternalType;
	AtomT *Type(void) { return *InternalType; }

	llvm::Value *Target = nullptr;

	void GenerateAllocate(llvm::BasicBlock *Block)
	{
		auto RealType = dynamic_cast<TypeBaseT *>(*InternalType);
		Target = new llvm::AllocaInst(RealType->GenerateType(), "", Block);
	}

	llvm::Value *GenerateLoad(llvm::BasicBlock *Block) 
	{ 
		return new llvm::LoadInst(Target, "", Block);
	}

	void GenerateStore(llvm::BasicBlock *Block, llvm::Value *Value)
	{
		new llvm::StoreInst(Value, Target, Block);
	}

	DynamicT(AtomT *Type) : InternalType(Type) {}
};

struct DynamicArgumentT : ExpressionBaseT
{
	FunctionInstanceT *Parent;
	size_t Index;
	llvm::Value *GenerateLoad(llvm::BasicBlock *Block) 
	{ 
		auto Iterator = Parent->Target->getArgumentList().begin();
		std::advance(Iterator, Index);
		return *Iterator;
	}
};

struct IntT : ExpressionBaseT
{
	int Value;

	SingleT InternalType;
	AtomT *Type(void) { return *(InternalType ? InternalType : InternalType = new IntTypeT); }
	
	llvm::Value *GenerateLoad(llvm::BasicBlock *Block) { return llvm::ConstantInt::get(LLVMInt(32), Value, true); }

	IntT(int const &Value) : Value(Value) {}
};

struct StringT : ExpressionBaseT
{
	std::string Value;
	
	SingleT InternalType;
	AtomT *Type(void) { return *(InternalType ? InternalType : InternalType = new StringTypeT); }
	
	llvm::Value *GenerateLoad(llvm::BasicBlock *Block) { return llvm::ConstantDataArray::getString(llvm::getGlobalContext(), Value.c_str()); }

	StringT(std::string const &Value) : Value(Value) {}
};

struct SimpleAssignmentT : ExpressionBaseT
{
	SingleT Target; // Must simplify to const string
	SingleT Value;
	
	void Simplify(void)
	{
		if (!Target) assert(0 && "Error");
		Target->Simplify();
		if (!Value) assert(0 && "Error");
		Value->Simplify();
	}
};

struct RecordT : AtomT
{
	std::vector<std::pair<std::string, SingleT>> Elements;
	
	void Add(std::string const &Name, AtomT *Item) 
	{
		for (auto &Element : Elements) assert(Element.first() != Name);
		Elements.emplace_back(Name, Item);
	}
	
	AtomT *Get(std::string const &Name)
	{
		for (auto &Element : Elements) if (Element.first() == Name) return Element.second();
		return nullptr;
	}
	
	void Simplify(void)
	{
		for (auto &Statement : Statements)
		{
			auto Assignment = dynamic_cast<SimpleAssignmentT *>(*Statement);
			if (!Assignment) assert(0 && "Error");
			Assignment->Target->Simplify();
			auto Name = dynamic_cast<StringT *>(*Assignment->Target);
			if (!Name) assert(0 && "Error");
			Elements.push_back(Name, Assignment->Value);
		}
	}
	
	RecordT(std::vector<AtomT *> const &Statements) : Statements(Statements) {}
};

struct ElementT : AtomT
{
	SingleT Parent;
	SingleT Base;
	SingleT Name;

	void Simplify(void)
	{
		if (!Base) Base = Parent;
		if (!Base) assert(0 && "Error");
		Base->Simplify();
		auto Record = dynamic_cast<RecordT *>(*Base);
		if (!Record) assert(0 && "Error");

		Name->Simplify();
		auto String = dynamic_cast<StringT *>(*Name);
		if (!Name) assert(0 && "Error");

		auto Found = Record->Get(String->Value);
		if (!Found)
		{
			Found = new UndefinedT;
			Record->Add(String->Value, Found);
		}
		Replace(Found);
	}

	ElementT(AtomT *Parent, AtomT *Base, AtomT *Name) : Parent(Parent), Base(Base), Name(Name) {}
};

enum struct AllocExistenceT
{
	AutoE,
	StaticE,
	DynamicE
};

struct AssignmentT : AtomT
{
	AllocExistenceT Existence;
	SingleT Target;
	SingleT Value;
	
	bool Undefined = false;

	void Simplify(void)
	{
		if (!Target) assert(0 && "Error");
		Target->Simplify();

		if (!Value) assert(0 && "Error");
		Value->Simplify();

		if (!Context)
		{
			assert(0 && "Error"); // TODO globals
			// Note, only global constants should be supported, no dynamics
		}
		else if (dynamic_cast<FunctionT *>(Context))
		{
			if (dynamic_cast<UndefinedT *>(*Target))
			{
				auto ValueType = dynamic_cast<TypeBaseT *>(Value->Type());
				if (!ValueType) assert(0 && "Error");

				Undefined = true;

				if (IsDynamic(ValueType->Existence) || (Existence != AllocExistenceT::AutoE))
				{
					if (Existence != AllocExistenceT::AutoE)
					{
						ValueType = ValueType->Clone();
						if (Existence == AllocExistenceT::StaticE) 
							ValueType->Existence = ExistenceT::StaticE;
						else if (Existence == AllocExistenceT::DynamicE) 
							ValueType->Existence = ExistenceT::DynamicE;
					}
					auto Dynamic = new DynamicT(ValueType);
					Target->Replace(Dynamic);
				}
				else
				{
					Target->Replace(Value);
					Replace(nullptr);
					return;
				}
			}
			else
			{
				auto TargetType = dynamic_cast<TypeBaseT *>(Target->Type());
				if (!TargetType) assert(0 && "Error");

				auto ValueType = dynamic_cast<TypeBaseT *>(Value->Type());
				if (!ValueType) assert(0 && "Error");
				
				if (*TargetType != *ValueType) assert(0 && "Error");
				if (TargetType->Existence == ExistenceT::StaticE) assert(0 && "Error");

				if (IsConstant(TargetType->Existence) && (Existence == AllocExistenceT::AutoE))
				{
					if (!IsConstant(ValueType->Existence)) assert(0 && "Error");
					Target->Replace(Value);
					Replace(nullptr);
					return;
				}
			}
		}
		else if (dynamic_cast<RecordT *>(Context))
		{
			assert(0 && "Error"); // TODO
		}
		else assert(0 && "Error"); // Unknown context
	}
	
	void GenerateStatement(llvm::BasicBlock *Block)
	{
		if (!Context)
		{
			assert(0 && "Error"); // Unsupported
		}
		else if (dynamic_cast<FunctionT *>(Context))
		{
			auto Dynamic = dynamic_cast<DynamicT *>(*Target);
			if (Undefined) Dynamic->GenerateAllocate(Block);
			auto Expression = dynamic_cast<ExpressionBaseT *>(*Value);
			Dynamic->GenerateStore(Block, Expression->GenerateLoad(Block));
		}
		else if (dynamic_cast<RecordT *>(Context))
		{
			// TODO
		}
		else assert(0 && "Error");
	}

	AssignmentT(AllocExistenceT Existence, AtomT *Target, AtomT *Value) : Existence(Existence), Target(Target), Value(Value) {}
};

struct MathAddT : ExpressionBaseT
{
	SingleT Left;
	SingleT Right;

	template <typename ConstTypeT> bool Operate(AtomT *Left, AtomT *Right)
	{
		auto SpecificLeft = dynamic_cast<ConstTypeT *>(Left);
		auto SpecificRight = dynamic_cast<ConstTypeT *>(Right);
		assert(!SpecificLeft == !SpecificRight);
		if (!SpecificLeft) return false;
		Replace(new ConstTypeT(SpecificLeft->Value + SpecificRight->Value));
		return true;
	}

	void Simplify(void)
	{
		if (!Left) assert(0 && "Error");
		Left->Simplify();
		if (!Right) assert(0 && "Error");
		Right->Simplify();

		auto LeftExpression = dynamic_cast<ExpressionBaseT *>(*Left);
		if (!LeftExpression) assert(0 && "Error");
		auto LeftType = dynamic_cast<TypeBaseT *>(LeftExpression->Type());
		if (!LeftType) assert(0 && "Error");
		
		auto RightExpression = dynamic_cast<ExpressionBaseT *>(*Right);
		if (!RightExpression) assert(0 && "Error");
		auto RightType = dynamic_cast<TypeBaseT *>(RightExpression->Type());
		if (!RightType) assert(0 && "Error");

		if (*LeftType != *RightType) assert(0 && "Error");

		if (IsConstant(LeftType->Existence) && IsConstant(RightType->Existence))
		{
			if (Operate<IntT>(Left, Right)) return;
			//if (Operate<FloatT>(Left, Right)) return;
			assert(0 && "Error"); // Bad type
		}
	}
	
	llvm::Value *GenerateLoad(llvm::BasicBlock *Block)
	{
		auto LeftExpression = dynamic_cast<ExpressionBaseT *>(*Left);
		if (!LeftExpression) assert(0 && "Error");
		
		auto RightExpression = dynamic_cast<ExpressionBaseT *>(*Right);
		if (!RightExpression) assert(0 && "Error");

		return llvm::BinaryOperator::Create(
			llvm::Instruction::Add, 
			LeftExpression->GenerateLoad(Block), 
			RightExpression->GenerateLoad(Block), 
			"", Block);
	}

	MathAddT(AtomT *Left, AtomT *Right) : Left(Left), Right(Right) {}
};*/

struct GenerateContextT
{
	llvm::Module *Module;
	llvm::BasicBlock *Block;
};

struct UndefinedT : AtomT
{
};

// Base classes
struct StatementBaseT : AtomT
{
	virtual void GenerateStatement(GenerateContextT Context) = 0;
};

struct ExpressionBaseT : AtomT
{
	virtual AtomT *Result(void) = 0;
	virtual void Allocate(AllocExistenceT Existence, UndefinedT *Target) = 0;
	virtual llvm::Value *GenerateLoad(GenerateContextT Context) = 0;
};

struct TypeBaseT : ExpressionBaseT
{
	virtual void Allocate(ExistenceT Existence, UndefinedT *Target) = 0;
	virtual llvm::Type *GenerateType(void) = 0;
};

struct RealBaseT : ExpressionBaseT
{
	virtual void Allocate(AllocExistenceT Existence, UndefinedT *Target) = 0;
	virtual void Assign(AtomT *Value) = 0;
	virtual void GenerateAllocate(GenerateContextT Context) { INTERNALERROR; }
	virtual void GenerateAssign(GenerateContextT Context, AtomT *Value) { INTERNALERROR; }
};

struct DynamicT : RealBaseT
{
	bool Static;
	SingleT Type;
	
	// Simplification state
	bool Unassigned = true;
	
	// Generation state
	llvm::Value *Target;
	
	void Allocate(AllocExistenceT Existence, UndefinedT *Target) override
	{
		bool Static = this->Static;
		if (Existence == AllocExistenceT::Dynamic) Static = false;
		else if (Existence == AllocExistenceT::Static) Static = true;
		Target->Replace(new DynamicT(Static, Type));
	}
	
	void Assign(AtomT *Value) override
	{
		if (Static && Assigned)
			COMPILEERROR;
		Assigned = true;
	}
	
	void GenerateAllocate(GenerateContextT Context) override
	{
		auto Type = dynamic_cast<TypeBaseT *>(*this->Type);
		Target = new llvm::AllocaInst(Type->GenerateType(), "", Context.Block);
	}
	
	void GenerateAssign(GenerateContextT Context, AtomT *Value) override
	{
		auto Value = dynamic_cast<RealBaseT *>(Value);
		assert(Target);
		new llvm::StoreInst(Value->GenerateLoad(Context), Target, Context.Block);
	}
	
	llvm::Value *GenerateLoad(GenerateContextT Context) override
	{
		assert(Target);
		return new llvm::LoadInst(Target, "", Context.Block);
	}
};

// Constants
struct IntTypeT : TypeBaseT
{
	llvm::Type *GenerateType(GenerateContextT Context) 
	{ 
		return LLVMInt(32); 
	}
};

struct ConstantBaseT : RealBaseT
{
	void Allocate(AllocExistenceT Existence, AtomT *Target) override
	{
		auto Type = Type->Clone();
		if (Existence == AllocExistenceT::Dynamic) 
			Target->Replace(new DynamicT(false, Type));
		else if (Existence == AllocExistenceT::Static) 
			Target->Replace(new DynamicT(true, Type));
		else Target->Replace(Clone());
	}
};

struct IntT : ConstantBaseT
{
	int Value;
	
	void Assign(AtomT *Value) override
	{
		auto Value = dynamic_cast<IntT *>(Value);
		this->Value = Value->Value;
	}
	
	llvm::Value *GenerateLoad(GenerateContextT Context)
	{ 
		return llvm::ConstantInt::get(LLVMInt(32), Value, true); 
	}
	
	IntT(int const &Value) : Value(Value) {}
};

struct StringTypeT : TypeBaseT
{
	llvm::Type *GenerateType(void) 
	{ 
		return llvm::PointerType::getUnqual(LLVMInt(8)); 
	}
};

struct StringT : ConstantBaseT
{
	std::string Value;
	
	llvm::Value *GenerateLoad(GenerateContextT Context) 
	{ 
		return llvm::ConstantDataArray::getString(llvm::getGlobalContext(), Value.c_str()); 
	}

	StringT(std::string const &Value) : Value(Value) {}
};

// Records...
struct RecordT : ValueBaseT
{
	bool IsStruct;
	MultipleT Assignments;
	
	std::vector<std::pair<std::string, SingleT>> Elements;

	void Add(std::string const &Name, AtomT *Item) 
	{
		for (auto &Element : Elements) assert(Element.first() != Name);
		Elements.emplace_back(Name, Item);
	}
	
	AtomT *Get(std::string const &Name)
	{
		for (auto &Element : Elements) if (Element.first() == Name) return Element.second();
		return nullptr;
	}
	
	void Simplify(void)
	{
		for (auto &Element : Elements)
			Element.second()->Simplify();
	}
	
	void Assign(AtomT *Other)
	{
		if (auto Undefined = dynamic_cast<UndefinedT>(Target))
		{
			auto *ScopeRecord = new RecordT(IsStruct);
			for (auto &Element : Elements)
			{
				SingleT NewElement = new UndefinedT;
				ScopeRecord->Add(Element.first(), NewElement);
				auto Value = dynamic_cast<ValueBaseT *>(*Element.second());
				Value->Assign(NewElement);
			}
			Undefined->Replace(ScopeRecord);
		}
	}
	
	void GenerateAssign(GenerateContextT Context)
	{
		auto ScopeRecord = dynamic_cast<DynamicT>(Target);
		Target->GenerateStore(Context, GenerateLoad(Module, Block));
	}
};

// Assignment and scope
struct ElementT : AtomT
{
	SingleT Parent;
	SingleT Base;
	SingleT Name;

	void Simplify(void)
	{
		if (!Base) Base = Parent;
		Base->Simplify();
		auto Record = dynamic_cast<RecordT *>(*Base);

		Name->Simplify();
		auto String = dynamic_cast<StringT *>(*Name);

		auto Found = Record->Get(String->Value);
		if (!Found)
		{
			Found = new UndefinedT;
			Record->Add(Name->Value, Found);
		}
		Replace(Found);
	}

	ElementT(AtomT *Parent, AtomT *Base, AtomT *Name) : Parent(Parent), Base(Base), Name(Name) {}
};

enum struct AllocExistenceT
{
	AutoE,
	StaticE,
	DynamicE
};

struct AssignmentT : StatementBaseT
{
	AllocExistenceT Existence;
	SingleT Target;
	SingleT Value;
	
	bool Allocate = false;
	
	void Simplify(void)
	{
		Target->Simplify();
		Value->Simplify();
		auto Value = dynamic_cast<RealBaseT>(*Value);
		if (dynamic_cast<UndefinedT *>(*Target))
		{
			Allocate = true;
			Value->Allocate(Existence, Target);
		}
		auto Target = dynamic_cast<RealBaseT>(*Target);
		Target->Assign(Value);
		if (!Value) Replace(nullptr);
	}
	
	void GenerateStatement(GenerateContextT Context)
	{
		auto Target = dynamic_cast<RealBaseT>(*Target);
		if (Allocate) Target->GenerateAllocate(Context);
		Target->GenerateAssign(Context, Value);
	}
};

// TODO
// Dynamic + ConstantAssign functions (instead of Assign)
// Target->GenerateAssign(Value) instead of opposite

///////////////////////////////////////////////
int main(int, char **)
{
	llvm::LLVMContext & context = llvm::getGlobalContext();
	llvm::Module *module = new llvm::Module("asdf", context);
	llvm::IRBuilder<> builder(context);

	llvm::FunctionType *funcType = llvm::FunctionType::get(builder.getVoidTy(), false);
	llvm::Function *mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module);
	llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
	
	MultipleT Statements;
	SingleT Scope = new RecordT;
	
	// Const record test
	Statements.emplace_back(new AssignmentT
	(
		AllocExistenceT::AutoE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("a")
		),
		new IntT(4)
	);
	Statements.emplace_back(new AssignmentT
	(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("b")
		),
		new IntT(17)
	);
	Statements.emplace_back(new AssignmentT
	(
		AllocExistenceT::AutoE,
		new ElementT
		(
			Scope,
			nullptr,
			new StringT("f")
		),
		new RecordT
		(
			new SimpleAssignmentT
			(
				new StringT("m1"),
				new IntT(47)
			),
			new SimpleAssignmentT
			(
				new StringT("m2"),
				new ElementT
				(
					Scope,
					nullptr,
					new StringT("a")
				)
			),
			new SimpleAssignmentT
			(
				new StringT("m3"),
				new ElementT
				(
					Scope,
					nullptr,
					new StringT("b")
				)
			)
		)
	));
	Statements.emplace_back(new AssignmentT
	(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			new ElementT(
				Scope,
				nullptr, 
				new StringT("f")
			),
			new StringT("m4")
		),
		new IntT(39)
	));
	Statements.emplace_back(new AssignmentT
	(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			nullptr,
			new StringT("z")
		),
		new ElementT(
			Scope,
			new ElementT(
				Scope,
				nullptr, 
				new StringT("f")
			),
			new StringT("m4")
		)
	));
	
	// Dynamic record test
	
	// Function test
	/*Statements.emplace_back(new AssignmentT(
		AllocExistenceT::AutoE,
		new ElementT
		(
			Scope,
			nullptr,
			new StringT("f")
		),
		new FunctionT
		(
			new FunctionTypeT
			(
				new SimpleAssignmentT
				(
					new StringT("in"),
					new IntTypeT()
				),
				new SimpleAssignmentT
				(
					new StringT("out"),
					new IntTypeT()
				)
			),
			{
				new AssignmentT
				(
					AllocExistenceT::AutoE,
					new ElementT
					(
						Scope,
						nullptr,
						new String("out")
					),
					new ElementT
					(
						Scope,
						nullptr,
						new String("in")
					)
				)
			}
		)
	));
	Statements.emplace_back(new AssignmentT(
		AllocExistenceT::AutoE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("z")
		),
		new MathAddT(
			new IntT(7),
			new IntT(5)
		)
	));
	Statements.emplace_back(new AssignmentT(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("a")
		),
		new CallT(
			ElementT(
				Scope,
				nullptr,
				new StringT("f")
			),
			ElementT(
				Scope,
				nullptr,
				new String("z")
			)
		)
	));*/
	/*Statements.emplace_back(new AssignmentT(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("a")
		),
		new ElementT(
			Scope,
			nullptr,
			new StringT("z")
		)
	));*/
	for (auto &Statement : Statements) Statement->Simplify();
	for (auto &Statement : Statements) 
		if (Statement) dynamic_cast<AssignmentT *>(static_cast<AtomT *>(Statement))->GenerateStatement(entry);

	/*SingleT Test;
	SingleT Scope = new RecordT;
	Test = new AssignmentT(
		AllocExistenceT::DynamicE,
		new ElementT(
			Scope,
			nullptr, 
			new StringT("a")
		),
		new IntT(4)
	);
	Test->Simplify();
	dynamic_cast<AssignmentT *>(*Test)->GenerateStatement(entry);*/
	
	builder.CreateRetVoid();
	module->dump();
	return 0;
}

