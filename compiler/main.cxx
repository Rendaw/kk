/*NOTE NOTE NOTE NOTe
] test

*/

#include "type.h"

#include <string>
#include <functional>
#include <map>
#include <iostream>
#include <cassert>
#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#define __STDC_CONSTANT_MACROS 
#define __STDC_LIMIT_MACROS
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
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

namespace Core
{

#define ERROR assert(false)

//================================================================================================================
// Core
struct ContextT;
struct AtomT;
struct NucleusT
{
	private:
		friend struct AtomT;
		std::vector<AtomT *> Atoms;
	protected:
		void Replace(NucleusT *Replacement);
	public:
		virtual ~NucleusT(void);
		virtual void Simplify(ContextT Context);
		virtual AtomT GetType(void);
};

struct ContextT;
struct AtomT
{
	private:
		NucleusT *Nucleus;
	
	public:
		void Set(NucleusT *Nucleus)
		{
			Clear();
			this->Nucleus = Nucleus;
			if (Nucleus)
				Nucleus->Atoms.push_back(this);
		}
		
		void Clear(void)
		{
			if (Nucleus) 
			{
				Nucleus->Atoms.erase(std::find(Nucleus->Atoms.begin(), Nucleus->Atoms.end(), this));
				if (Nucleus->Atoms.empty())
					delete Nucleus;
				Nucleus = nullptr;
			}
		}
		
		AtomT(void) : Nucleus(nullptr) {}
		AtomT(NucleusT *Nucleus) { Set(Nucleus); }
		AtomT(AtomT const &Other) { Set(Other.Nucleus); }
		~AtomT(void) { Clear(); }
		
		AtomT &operator =(NucleusT *Nucleus) { Set(Nucleus); return *this; }
		AtomT &operator =(AtomT &Other) { Set(Other.Nucleus); return *this; }
	
		operator NucleusT *(void) { return Nucleus; }
		operator bool(void) { return Nucleus; }
		
		template <typename AsT> OptionalT<AsT *> As(void) 
		{ 
			auto Out = dynamic_cast<AsT *>(Nucleus);
			if (!Out) return {};
			return {Out};
		}
		
		NucleusT *operator ->(void) { return Nucleus; }
};

struct ContextT
{
	llvm::LLVMContext &LLVM;
	llvm::Module *Module;
	
	llvm::BasicBlock *Block;
	AtomT Scope;
	
	ContextT(
		llvm::LLVMContext &LLVM, 
		llvm::Module *Module, 
		llvm::BasicBlock *Block, 
		AtomT Scope) : 
		LLVM(LLVM), 
		Module(Module),
		Block(Block),
		Scope(Scope)
		{}
		
	ContextT(ContextT const &Context) : 
		LLVM(Context.LLVM), 
		Module(Context.Module),
		Block(Context.Block),
		Scope(Context.Scope)
		{}
		
	ContextT EnterScope(AtomT Scope)
	{
		return ContextT(
			LLVM,
			Module,
			Block,
			Scope);
	}
};

void NucleusT::Replace(NucleusT *Replacement)
{
	auto OldAtoms = Atoms;
	for (auto Atom : OldAtoms)
		*Atom = Replacement;
}
NucleusT::~NucleusT(void) {}
void NucleusT::Simplify(ContextT Context) {}
AtomT NucleusT::GetType(void) { return {nullptr}; }

//================================================================================================================
// Interfaces
struct ValueT
{
	virtual ~ValueT(void) {}
	virtual AtomT Allocate(ContextT Context) { ERROR; }
};

struct AssignableT
{
	virtual ~AssignableT(void) {}
	virtual void Assign(ContextT Context, AtomT Other) { ERROR; }
};

// LLVM
struct LLVMLoadableT
{
	virtual ~LLVMLoadableT(void) {}
	virtual llvm::Value *GenerateLLVMLoad(ContextT Context) = 0;
};

struct LLVMLoadableTypeT
{
	virtual ~LLVMLoadableTypeT(void) {}
	virtual llvm::Type *GenerateLLVMType(ContextT Context) = 0;
};

//================================================================================================================
// Basics
struct UndefinedT : NucleusT, AssignableT
{
	void Assign(ContextT Context, AtomT Other) override
	{
		auto Source = Other.As<ValueT>();
		if (!Source) ERROR;
		auto DestAtom = Source->Allocate(Context);
		auto Dest = DestAtom.As<AssignableT>();
		assert(Dest);
		Dest->Assign(Context, Other);
		Replace(DestAtom);
	}
};

struct ImplementT : NucleusT, ValueT
{
	AtomT Type, Value;
	
	AtomT Allocate(ContextT Context) override
	{
		auto Value = this->Value.As<ValueT>();
		if (!Value) ERROR;
		return Value->Allocate(Context);
	}
	
	void Simplify(ContextT Context) override
	{
		Type->Simplify(Context);
		Value->Simplify(Context);
	}
};

//================================================================================================================
// Primitives
struct PrimitiveTypeT 
{
	virtual ~PrimitiveTypeT(void) {}
	virtual AtomT Allocate(ContextT Context) = 0;
	virtual void Assign(ContextT Context, llvm::Value *Target, AtomT Other) = 0;
};

template <typename TypeT> struct PrimitiveMixinT : virtual NucleusT, virtual ValueT
{
	AtomT Type;
	
	AtomT GetType(void) override
	{
		if (!Type) Type = new TypeT;
		return Type;
	}
	
	AtomT Allocate(ContextT Context) override
	{
		auto Type = GetType().template As<PrimitiveTypeT>();
		if (!Type) ERROR;
		return Type->Allocate();
	}
};

struct StringTypeT;
struct StringT : virtual NucleusT, virtual ValueT, virtual AssignableT, PrimitiveMixinT<StringTypeT>
{
	std::string Data;
	
	void Assign(ContextT Context, AtomT Other) override
	{
		GetType().As<StringTypeT>().Assign(Data, Other);
	}
};

struct StringTypeT : NucleusT, PrimitiveTypeT
{
	uint16_t ID;
	// No dynamic strings, for now at least
	bool Static;

	void CheckType(bool Defined, AtomT Other)
	{
		if (Static && Defined) ERROR;
		auto OtherType = Other.GetType().As<StringTypeT>();
		if (!OtherType) ERROR;
		if (OtherType.ID != ID) ERROR;
	}
	
	AtomT Allocate(ContextT Context) override
	{
		return {new StringT(*this)};
	}
	
	void Assign(ContextT Context, std::string &Data, AtomT Other)
	{
		CheckType(Defined, Other);
		if (auto &Implementation = Other->As<ImplementT>())
		{
			auto String = Implementation.Value.As<StringT>();
			if (!String) ERROR;
			Data = String.Data;
		}
		else
		{
			auto String = Other.As<StringT>();
			if (!String) ERROR;
			Data = String.Data;
		}
		Defined = true;
	}
	
	void Assign(ContextT Context, llvm::Value *Target, AtomT Other) override { assert(false); }
};

struct NumericTypeT;
template <typename DataT> struct NumericT : 
	virtual NucleusT, 
	virtual ValueT, 
	virtual AssignableT, 
	virtual LLVMLoadableT, 
	PrimitiveMixinT<NumericTypeT>
{
	DataT Data;
	
	void Assign(ContextT Context, AtomT Other) override
	{
		if (auto Number = Other.As<NumericT<DataT>>())
		{
			Data = Number.Data;
		}
		else if (auto Implementation = Other.As<ImplementT>())
		{
			if (auto Number = Implementation.Value.As<NumericT<int32_t>>()) Data = Number.Data;
			else if (auto Number = Implementation.Value.As<NumericT<uint32_t>>()) Data = Number.Data;
			else if (auto Number = Implementation.Value.As<NumericT<float>>()) Data = Number.Data;
			else if (auto Number = Implementation.Value.As<NumericT<double>>()) Data = Number.Data;
			else ERROR;
		}
		else ERROR;
	}
	
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{
		return llvm::ConstantInt::get
			(
				llvm::IntegerType::get(Context.LLVM, sizeof(Data) * 8), 
				0, 
				false
			);
	}
};

struct DynamicT : NucleusT, AssignableT, LLVMLoadableT
{
	bool Defined;
	AtomT Type;
	llvm::Value *Target;
	
	AtomT Allocate(ContextT Context) override
	{
		auto Type = GetType()->As<TypeT>();
		if (!Type) ERROR:
		return Type->Allocate();
	}
	
	void Assign(ContextT Context, AtomT Other) override
	{
		auto Type = GetType()->As<TypeT>();
		if (!Type) ERROR:
		Type->Assign(Context, Defined, Target, Other);
	}
	
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{
		if (!Defined) ERROR;
		return new llvm::LoadInst(Target, "", Context.Block); 
	}
};

enum struct LLVMConversionTypeT { Signed, Unsigned, Float };
llvm::Value *GenerateLLVMNumericConversion(llvm::BasicBlock *Block, llvm::Value *Source, llvm::Type *SourceType, bool SourceSigned, llvm::Type *DestType, bool DestSigned)
{
	if (SourceType->IsIntegerTy())
	{
		if (SourceSigned)
		{
			if (DestType->IsIntegerTy())
			{
				if (DestType->GetIntegerBitWidth() < SourceType->GetIntegerBitWidth())
					return new llvm::TruncInst(Source, DestType, "", Block);
				else if (DestType->GetIntegerBitWidth() == SourceType->GetIntegerBitWidth())
					return Source;
				else 
					return new llvm::SExtInst(Source, DestType, "", Block);
			}
			else
			{
				assert(DestType->IsFloatTy() || DestType->IsDoubleTy());
				return new llvm::SIToFPInst(Source, DestType, "", Block);
			}
		}
		else
		{
			if (DestType->IsIntegerTy())
			{
				if (DestType->GetIntegerBitWidth() < SourceType->GetIntegerBitWidth())
					return new llvm::TruncInst(Source, DestType, "", Block);
				else if (DestType->GetIntegerBitWidth() == SourceType->GetIntegerBitWidth())
					return Source;
				else 
					return new llvm::ZExtInst(Source, DestType, "", Block);
			}
			else
			{
				assert(DestType->IsFloatTy() || DestType->IsDoubleTy());
				return new llvm::UIToFPInst(Source, DestType, "", Block);
			}
		}
	}
	else
	{
		if (SourceType->IsFloatTy())
		{
			if (DestType->IsFloatTy())
			{
				return Source;
			}
			else
			{
				assert(DestType->IsDoubleTy());
				return new llvm::FPExtInst(Source, DestType, "", Block);
			}
		}
		else
		{
			assert(SourceType->IsDoubleTy());
			if (DestType->IsFloatTy())
			{
				return new llvm::FPTruncInst(Source, DestType, "", Block);
			}
			else
			{
				assert(DestType->IsDoubleTy());
				return Source;
			}
		}
	}
}

struct NumericTypeT : NucleusT, PrimitiveTypeT, LLVMLoadableTypeT
{
	uint16_t ID;
	bool Constant;
	bool Static;
	enum struct DataTypeT { Int, UInt, Float, Double } DataType;
	
	bool IsSigned(void) const
	{
		return DataType == DataTypeT::Int;
	}
	
	void CheckType(bool Defined, AtomT Other)
	{
		if (Static && Defined) ERROR;
		auto OtherType = Other.GetType().As<PrimitiveTypeBaseT<BaseT>>();
		if (!OtherType) ERROR;
		if (OtherType.ID != ID) ERROR;
		if (ID == DefaultTypeID)
		{
			if (Constant && !OtherType.Constant) ERROR;
			if (Other.DataType != DataType) ERROR;
		}
		else
		{
			assert(!Constant || OtherType.Constant);
			assert(DataType == OtherType.DataType);
		}
	}
	
	AtomT Allocate(ContextT Context) override
	{
		if (Constant)
		{
			switch (DataType)
			{
				case DataTypeT::Int: return new NumericT<int32_t>(*this);
				case DataTypeT::UInt: return new NumericT<uint32_t>(*this);
				case DataTypeT::Float: return new NumericT<float>(*this);
				case DataTypeT::Double: return new NumericT<double>(*this);
				default: assert(false); return nullptr;
			}
		}
		else return new DynamicT(*this, new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.Block));
	}
	
	void Assign(ContextT Context, bool &Defined, llvm::Value *Target, AtomT Other) override
	{
		CheckType(Defined, Other);
		llvm::Value *Source = nullptr;
		if (auto Loadable = Other->As<LLVMLoadableT>())
		{
			Source = Loadable->GenerateLLVMLoad();
		}
		else if (auto Implementation = Other.As<ImplementT>())
		{
			auto Loadable = Implementation.Value.As<LLVMLoadableT>();
			if (!Loadable) ERROR;
			Source = Loadable->GenerateLLVMLoad();
			auto OtherType = Implementation.Value.GetType().As<NumericTypeT>();
			if (!OtherType) ERROR;
			llvm::Type *SourceType = OtherType->GenerateLLVMType();
			bool SourceSigned = OtherType->IsSigned();
			llvm::Type *DestType = GenerateLLVMType();
			bool DestSigned = IsSigned();
			Source = GenerateLLVMNumericConversion(
				Context.Block,
				Source,
				SourceType,
				SourceSigned,
				DestType,
				DestSigned);
		}
		else ERROR;
		new llvm::StoreInst(Source, Target, Context.Block);
		Defined = true;
	}
	
	llvm::Type *GenerateLLVMType(ContextT Context) override
	{
		switch (DataType)
		{
			case DataTypeT::Int: 
			case DataTypeT::UInt:
				return llvm::IntegerType::get(Context.LLVM, 32);
			case DataTypeT::Float:
				return llvm::Type::getFloatTy(Context.LLVM);
			case DataTypeT::Double:
				return llvm::Type::getDoubleTy(Context.LLVM);
			default: assert(false); return nullptr;
		}
	}
	
};

//================================================================================================================
// Groups
struct GroupCollectionT
{
	std::map<std::string, AtomT> Keys;

	OptionalT<AtomT> GetByKey(std::string const &Key)
	{
		auto Found = Keys.find(Key);
		if (Found == Keys.end()) return {};
		return Found->second;
		//return Values[*Found];
	}

	void Add(std::string const &Key, AtomT Value)
	{
		auto Found = Keys.find(Key);
		if (Found != Keys.end()) ERROR;
		//auto Index = Values.size();
		//Values.push_back(Value);
		//Keys[Key] = Index;
		Keys[Key] = Value;
		return;
	}
	
	auto begin(void) -> decltype(Keys.begin()) { return Keys.begin(); }
	auto end(void) -> decltype(Keys.end()) { return Keys.end(); }
};

struct GroupT : NucleusT, ValueT, AssignableT, GroupCollectionT
{
	std::vector<AtomT> Statements;

	void Simplify(ContextT Context) override
	{
		auto NewContext = Context.EnterScope(this);
		for (auto &Statement : Statements)
			Statement->Simplify(NewContext);
	}
	
	AtomT Allocate(ContextT Context) override
	{
		auto NewContext = Context.EnterScope(this);
		auto Out = new GroupT;
		for (auto &Pair : *this)
		{
			auto Value = Pair.second.As<ValueT>();
			if (!Value) ERROR;
			Out->Add(Pair.first, Value->Allocate(NewContext));
		}
		return {Out};
	}

	void Assign(ContextT Context, AtomT Other) override
	{
		auto NewContext = Context.EnterScope(this);
		auto Group = Other.As<GroupT>();
		if (!Group) ERROR;
		for (auto &Pair : *this)
		{
			auto Dest = Pair.second.As<AssignableT>();
			if (!Dest) ERROR;
			auto Source = GetByKey(Pair.first);
			if (!Source) ERROR;
			Dest->Assign(NewContext, *Source);
		}
	}

	AtomT AccessElement(AtomT Key)
	{
		auto KeyString = Key.As<StringT>();
		if (!KeyString) ERROR;
		auto Got = GetByKey(KeyString->Data);
		if (Got) return *Got;
		auto Out = new UndefinedT;
		Add(KeyString, Out);
		return Out;
	}
};

struct ElementT : NucleusT
{
	AtomT Base, Key;

	void Simplify(ContextT Context) override
	{
		if (!Base) Base = Context.Scope;
		Base->Simplify(Context);
		Key->Simplify(Context);
		auto Group = Base.As<GroupT>();
		if (!Group) ERROR;
		Replace(Group->AccessElement(Key));
	}
};

//================================================================================================================
// Statements
struct AssignmentT : NucleusT
{
	AtomT Left, Right;

	void Simplify(ContextT Context) override
	{
		Left.Simplify();
		Right.Simplify();
		Left.Assign(Right);
	}
};

//================================================================================================================
// Functions

/*struct FunctionT
{
	AtomT Call(AtomT Input)
	{
		auto Result = Function.Refine(Input);
		if (auto DynamicFunction = Result.As<DynamicFunctionT>())
		{
			// If return value is struct, allocate + set as first of DynamicInput
			// Pull out linearization of dynamic elements from Input -> DynamicInput
			// Result = LLVM call create
			// If return value is struct, override - Result = DynamicT(Allocated)
		}
		Replace(Result);
	}
	
	AtomT Refine(AtomT Input)
	{
		// Generate refinement ID from input
		// If ID in memory, return that value
		// Recurse input, TODO
	}
};

struct CallT
{
	AtomT Function, Input;
	
	void Simplify(ContextT Context)
	{
		if (!Function) ERROR;
		Function.Simplify(Context);
		if (!Input) ERROR;
		Input.Simplify(Context);
		auto Function = Function.As<FunctionT>();
		if (!Function) ERROR:
		Replace(Function.Call(Input));
	}
};*/

}

using namespace Core;

//================================================================================================================
// Main
int main(int, char **)
{
	llvm::ReturnInst::Create(LLVM, Block);
	auto &LLVM = llvm::getGlobalContext();
	auto Module = new llvm::Module("asdf", LLVM);
	auto MainType = llvm::FunctionType::get(llvm::Type::getVoidTy(LLVM), false);
	auto Main = llvm::Function::Create(MainType, llvm::Function::ExternalLinkage, "main", Module);
	auto Block = llvm::BasicBlock::Create(LLVM, "entrypoint", Main);
	
	auto MainGroup = new GroupT;
	auto MakeString = [&](std::string const &Value)
	{
		auto Out = new StringT;
		Out->Data = Value;
		return Out;
	};
	auto MakeInt = [&](int Value)
	{
		auto Out = new NumericT<int>;
		Out->Data = Value;
		return Out;
	};
	auto MakeFloat = [&](float Value)
	{
		auto Out = new NumericT<float>;
		Out->Data = Value;
		return Out;
	};
	auto MakeElement = [&](std::string const &Key)
	{
		auto Out = ElementT;
		Out->Key = MakeString(Key);
		return Out;
	};
	auto MakeDynamicInt = [&](NucleusT *&&Value)
	{
		auto Type = new NumericTypeT;
		Type->DataType = NumericTypeT::DataTypeT::Int;
		Type->Constant = false;
		auto Out = new ImplementT;
		Out->Type = Type;
		Out->Value = Value;
		return Out;
	};
	auto MakeIntCast = [&](NucleusT *&&Value)
	{
		auto Type = new NumericTypeT;
		Type->DataType = NumericTypeT::DataTypeT::Int;
		auto Out = new ImplementT;
		Out->Type = Type;
		Out->Value = Value;
		return Out;
	};
	auto MakeAssignment = [&](std::string const &Key, NucleusT *&&Value)
	{
		auto Assignment = new AssignmentT;
		Assignment->Left = MakeElement(Key);
		Assignment->Right = Value;
		MainScope.Statements.push_back(Assignment);
	};
	MakeAssignment("a", MakeString("hi"));
	MakeAssignment("b", MakeString("bye"));
	MakeAssignment("b", MakeElement("a"));
	MakeAssignment("c", MakeInt(40));
	MakeAssignment("d", MakeDynamicInt(35));
	MakeAssignment("d", MakeElement("c"));
	MakeAssignment("c", MakeIntCast(MakeFloat(17.3)));
	MakeAssignment("d", MakeElement("c"));
	MainScope->Simplify({LLVM, Module, Memcpy, Block, nullptr});
	
	Module->dump();
	delete MainGroup;
	
	return 0;
}

