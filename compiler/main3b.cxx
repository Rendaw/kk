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
	AtomT *Context = nullptr;
  
	std::list<SingleT *> References;
	void Replace(AtomT *Other);
	
	virtual AtomT *Type(void) { return nullptr; }
	virtual void Simplify(void) {}
	virtual AtomT *Clone(void) = 0;
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

struct UndefinedT : AtomT 
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

struct FunctionTypeT : TypeBaseT
{
	SingleT Input; // Simple assignment
	SingleT Output; // ""
	
	void Simplify(void)
	{
		if (!Input) assert(0 && "Error");
		Input->Simplify();
		if (!Output) assert(0 && "Error");
		Output->Simplify();
	}
	
	void Refine(FunctionInstanceT *Instance, AtomT *Refinement)
	{
		auto RefinedType = new FunctionTypeT;
		if (Input)
		{
			auto Input = dynamic_cast<SimpleAssignmentT *>(*Input);
			auto InputValue = dynamic_cast<TypeBaseT *>(*Input->Value);
			
			size_t ArgumentSequence = 0;
			std::function<void(AtomT *TemplateInput, AtomT *NewInput, AtomT *Refinement, RecordT *Scope)> Subrefine;
			Subrefine = [&](AtomT *TemplateInput, AtomT *Refinement, SingleT &NewInput, SingleT &NewScope)
			{
				assert(TemplateInput);
				if (auto TemplateRecord = dynamic_cast<RecordT *>(TemplateInput))
				{
					auto RefinementRecord = dynamic_cast<RecordT *>(Refinement);
					NewScope = new RecordT;
					NewInput = new RecordT;
					
					for (auto &Element : TemplateRecord->Items)
					{
						SingleT SubNewScope, SubNewInput;
						Subrefine(
							Element->Value, 
							RefinementRecord->Get(Element->Name), 
							SubNewInput,
							SubNewScope);
						if (!SubNewScope) assert(0 && "Error");
						NewScopeItem->Add(Element->Name, SubNewScope);
						if (SubNewInput)
							NewInput->Add(Element->Name, SubNewInput);
					}
					for (auto &Element : Record)
				}
				else if (dynamic_cast<TypeBaseT *>(TemplateInput->Value))
				{
					if (Refinement && IsConstant(Refinement->GetType()))
					{
						if (*Refinement->GetType()->Equal(TemplateInput->Value)) assert(0 && "Error");
						NewScopeItem = Refinement;
					}
					else
					{
						NewScopeItem = new UndefinedT;
						Instance->Statements.push_back(
							new AssignmentT(
								ScopeItem,
								new DynamicArgument(Instance, ArgumentSequence++));
						NewInput = TemplateInput->Clone();
					}
				}
				else assert(0 && "Error"); // Only types and records of types ok
			};
			SingleT NewInput;
			SingleT NewScope;
			Subrefine(InputValue, Refinement, NewInput, NewScope);

			if (NewScope)
				Instance->Scope->Add(Input->Target, NewScope);
			if (NewInput)
				RefinedType->Input = new SimpleAssignment(Input->Name, NewInput);
		}

		RefinedType->Output = Output;
		Instance->Type = RefinedType;
	}
	
	llvm::Type *Target = nullptr;
	llvm::Type *GenerateType(void)
	{
		if (Target) return Target;
		auto OutputType = llvm::Type::getVoidTy(llvm::getGlobalContext());
		std::vector<llvm::Type *> InputTypes;

		if (auto Output = dynamic_cast<SimpleAssignmentT *>(this->Output))
		{
			if (auto Record = dynamic_cast<RecordT *>(Output->Value))
				InputTypes.push_back(Record->GenerateType());
			else if (auto Type = dynamic_cast<TypeBaseT *>(Output->Value))
				OutputType = Type->GenerateType();
			else assert(0 && "Error");
		}

		if (auto Input = dynamic_cast<SimpleAssignmentT *>(this->Input))
		{
			std::function<void(AtomT *Input)> AggregateInputTypes;
			AggregateInputTypes = [&](AtomT *Input)
			{
				assert(Input);
				if (auto Record = dynamic_cast<RecordT *>(TemplateInput))
					for (auto &Element : Record->Items)
						AggregateInputItems(Element->Value);
				else if (Type = dynamic_cast<TypeBaseT *>(TemplateInput))
					InputTypes.push_back(Type->GenerateType();
				else assert(0 && "Error");
			};
			AggregateInputTypes(Input->Value);
		}

		return Target = llvm::FunctionType::get(OutputType, InputTypes, false);
	}
	
	llvm::Value *GenerateFunction(llvm::Module *Module, llvm::BasicBlock *Block)
	{
		auto Function = llvm::Function::Create(
			GenerateType(), 
			llvm::Function::PrivateLinkage, 
			"", 
			Module);
		
		if (auto Output = dynamic_cast<SimpleAssignmentT *>(this->Output))
			if (auto Record = dynamic_cast<RecordT *>(Output->Value))
				Function->getAttributes().addAttribute(llvm::getGlobalContext(), 0, llvm::Attribute::StructRet);
	
		return Function;
	}

	llvm::Value *GenerateCall(llvm::Module *Module, llvm::BasicBlock *Block, AtomT *Function, AtomT *Input)
	{
		llvm::Value *StructResult;
		
		std::vector<llvm::Value *> InputValues;
		
		if (auto Output = dynamic_cast<SimpleAssignmentT *>(this->Output))
		{
			if (auto Record = dynamic_cast<RecordT *>(Output->Value))
			{
				StructResult = Record->GenerateAlloc();
				InputValues.push_back(StructResult);
			}
		}

		if (Input)
		{
			std::function<void(AtomT *Input)> AggregateInputValues;
			AggregateInputValues = [&](AtomT *Input)
			{
				assert(Input);
				if (auto Record = dynamic_cast<RecordT *>(Input))
					for (auto &Element : Record->Items)
						AggregateInputItems(Element->Value);
				else if (Value = dynamic_cast<ExpressionBaseT *>(nput))
					InputValues.push_back(Value->GenerateLoad());
				else assert(0 && "Error");
			};
			AggregateInputValues(Input);
		}
		
		auto Instance = dynamic_cast<FunctionInstanceT *>(Function);
		if (!Instance) assert(0 && "Error");
		auto Call = llvm::CallInst::Create(Instance, InputValues, "", Block);
		return StructResult ? StructResult : Call;
	}
};

struct FunctionT
{
	SingleT Type;
	MultipleT Statements;
	
	void Simplify(void) 
	{ 
		if (!Type) assert(0 && "Error");
		Type->Simplify();
	}
	
	FunctionInstanceT *Refine(RecordT *Refinement)
	{
		SingleT Out = new FunctionInstanceT;
		auto Type = dynamic_cast<FunctionTypeT *>(*this->Type);
		if (!Type) assert(0 && "Error");
		Type->Refine(Out, Refinement);
		for (auto &Statement : Statements)
			Out->Statements.push_back(Statement->Clone());
		
		return Out;
	}
};

struct FunctionInstanceT : ExpressionBaseT
{
	// Cannot (must not) be assigned.  Created in-place when exporting, assigning a dynamic function, or calling
	SingleT Type;
	MultipleT Statements;
	
	SingleT Scope = new RecordT;
	SingleT OutputAtom;
	
	void Simplify(void)
	{
		for (auto &Statement : Statements)
			Statement->Simplify();
		
		// If all statements have been simplified out of existence (only possible for constants), replace the function with the result
		auto FunctionType = dynamic_cast<FunctionTypeT *>(*Type);
		if (auto Output = dynamic_cast<SimpleAssignmentT *>(*FunctionType->Output))
		{
			auto Name = dynamic_cast<StringT *>(*Output->Target);
			bool TotallySimplified = true;
			for (auto &Statement : Statements)
				if (Statement) { TotallySimplified = false; break; }
			if (TotallySimplified) 
				Replace(Scope->Get(Name->Value));
		}
	}
	
	void GenerateTarget(llvm::Module *Module, llvm::BasicBlock *Block)
	{
		assert(!Target);
		auto FunctionType = dynamic_cast<FunctionTypeT *>(*Type);
		Target = FunctionType->GenerateFunction(Module, Block);
		auto *Entry = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entrypoint", Target);
		for (auto &Statement : Statements)
			if (Statement) Statement->GenerateStatement(Entry);
		if (OutputAtom)
		{
			auto Expression = dynamic_cast<ExpressionBaseT *>(*OutputAtom);
			llvm::ReturnInst::Create(llvm::getGlobalContext(), Expression->GenerateLoad(), Entry);
		}
		else llvm::ReturnInst::Create(llvm::getGlobalContext(), Entry);
	}
	
	void GenerateLoad(llvm::Module *Module, llvm::BasicBlock *Block)
	{
		if (!Target) GenerateTarget(Module, Block);
		return Target;
	}
};

struct CallT : ExpressionBaseT
{
	SingleT Target;
	SingleT Input;
	
	void Simplify(void)
	{
		Target->Simplify();
		Input->Simplify();
		auto Function = dynamic_cast<FunctionT *>(*Target);
		Target = Function->Refine(Input);
		Target->Simplify();
		if (!dynamic_cast<FunctionInstanceT *>(*Target))
			Replace(Target);
	}
	
	void GenerateLoad(llvm::Module *Module, llvm::BasicBlock *Block)
	{
		auto Function = dynamic_cast<ExpressionBaseT *>(*Target);
		auto Type = dynamic_cast<FunctionTypeT *>(Target->GetType());
		Type->GenerateCall(Module, Block, Function->GenerateLoad(Block), Input);
	}
	
	CallT(AtomT *Target, AtomT *Input) : Target(Target), Input(Input) {}
}

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
	));
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

