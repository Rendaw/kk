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

// Common types
/*struct PositionPartT
{
	std::string Key;
	int Index;

	PositionPartT(void) : Index(-1) {}

	bool operator !(void) const { return Key.empty() && (Index < 1); }

	PositionPartT(std::string const &Key) : Key(Key), Index(-1) { assert(!Key.empty()); }

	PositionPartT(int const &Index) : Index(Index) { assert(Index >= 0); }

	json_object *Get(json_object *Source) const
	{
		if (!*this) { assert(false); return nullptr; }
		if (Key.empty())
		{
			assert(json_object_is_type(Source, json_type_array));
			return json_object_array_get_idx(Source, Index);
		}
		else
		{
			assert(json_object_is_type(Source, json_type_object));
			return json_object_object_get(Source, Key.c_str());
		}
	}

	std::string AsString(void) const
	{
		if (!*this) return "(invalid)";
		else if (Key.empty()) return String() << Index;
		else return Key;
	}
};

struct FullPositionT : std::enable_shared_from_this<FullPositionT>
{
	std::shared_ptr<FullPositionT> operator +(PositionPartT const &NewPart) const { return std::shared_ptr<FullPositionT>(new FullPositionT(shared_from_this(), Part)); }

	std::string AsString(void) const
	{
		std::list<std::string> Parts;
		for (auto Part = this; Part->Parent.get(); Part = Part->Parent.get()) Parts.push_front(Part->Part.AsString());
		String Out;
		for (auto const &Part : Parts) Out << "/" << Part;
		return Out;
	}
	protected:
		friend std::shared_ptr<FullPositionT> NewPosition(void);
		FullPositionT(void) {}
		FullPositionT(std::shared_ptr<FullPositionT const> const &Parent, PositionPartT const &Part) : Parent(Parent), Part(Part) {}
	
		std::shared_ptr<FullPositionT const> Parent;
		PositionPartT const Part;
};
typedef std::shared_ptr<FullPositionT> PositionT;
PositionT NewPosition(void) { return std::shared_ptr<FullPositionT>(new FullPositionT); }

typedef std::string ParseErrorMessageT;

struct ParseErrorT
{
	PositionT Position;
	ParseErrorMessageT Message;
	ParseErrorT(void) {}
	ParseErrorT(PositionT const &Position, std::string const &Message) : Position(Position), Message(Message) {}
	bool operator !(void) { return !Position; }
};*/

///////////////////////////////////////////////////////
inline llvm::Type *LLVMInt(unsigned int Size) { return llvm::IntegerType::get(llvm::getGlobalContext(), Size); }

struct SingleT;
struct AtomT : ShareableT
{
	std::list<SingleT *> References;
	void Replace(AtomT *Other);
	
	virtual AtomT *Type(void) { return nullptr; }
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

struct UndefinedT : AtomT {};

enum struct ExistenceT { ConstantE, StaticE, DynamicE };
bool IsConstant(ExistenceT const &Value) { return Value == ExistenceT::ConstantE; }
bool IsDynamic(ExistenceT const &Value) { return (Value == ExistenceT::StaticE) || (Value == ExistenceT::DynamicE); }

struct TypeBaseT : AtomT
{
	ExistenceT Existence;
	virtual TypeBaseT *Clone(void) = 0;
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

struct RecordT : AtomT
{
	MultipleT Assignments;

	std::vector<SingleT> ByIndex;
	std::map<std::string, SingleT> ByName;
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

		auto Found = Record->ByName.find(String->Value);
		if (Found == Record->ByName.end())
		{
			Record->ByIndex.push_back(new UndefinedT);
			auto Emplaced = Record->ByName.emplace(String->Value, Record->ByIndex.back());
			Found = Emplaced.first;
		}
		Replace(*Found->second);
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
	
	void GenerateStatement(llvm::BasicBlock *Block)
	{
		auto Dynamic = dynamic_cast<DynamicT *>(*Target);
		if (Undefined) Dynamic->GenerateAllocate(Block);
		auto Expression = dynamic_cast<ExpressionBaseT *>(*Value);
		Dynamic->GenerateStore(Block, Expression->GenerateLoad(Block));
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
};

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
		new ElementT(
			Scope,
			nullptr,
			new StringT("z")
		)
	));
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

