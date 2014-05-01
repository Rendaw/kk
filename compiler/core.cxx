#include "core.h"

namespace Core
{

#define ERROR assert(false)

//================================================================================================================
// Core
void NucleusT::Replace(NucleusT *Replacement)
{
	auto OldAtoms = Atoms;
	for (auto Atom : OldAtoms)
		*Atom = Replacement;
}

NucleusT::~NucleusT(void) {}

void NucleusT::Simplify(ContextT Context) {}

AtomT NucleusT::GetType(void) { return {nullptr}; }

void AtomT::Set(NucleusT *Nucleus)
{
	Clear();
	this->Nucleus = Nucleus;
	if (Nucleus)
		Nucleus->Atoms.push_back(this);
}

void AtomT::Clear(void)
{
	if (Nucleus) 
	{
		Nucleus->Atoms.erase(std::find(Nucleus->Atoms.begin(), Nucleus->Atoms.end(), this));
		if (Nucleus->Atoms.empty())
			delete Nucleus;
		Nucleus = nullptr;
	}
}

AtomT::AtomT(void) : Nucleus(nullptr) {}
AtomT::AtomT(NucleusT *Nucleus) { Set(Nucleus); }
AtomT::AtomT(AtomT const &Other) { Set(Other.Nucleus); }
AtomT::~AtomT(void) { Clear(); }

AtomT &AtomT::operator =(NucleusT *Nucleus) { Set(Nucleus); return *this; }
AtomT &AtomT::operator =(AtomT &Other) { Set(Other.Nucleus); return *this; }

AtomT::operator NucleusT *(void) { return Nucleus; }
AtomT::operator bool(void) { return Nucleus; }

NucleusT *AtomT::operator ->(void) { return Nucleus; }

ContextT::ContextT(
	llvm::LLVMContext &LLVM, 
	llvm::Module *Module, 
	llvm::BasicBlock *Block, 
	AtomT Scope) : 
	LLVM(LLVM), 
	Module(Module),
	Block(Block),
	Scope(Scope)
	{}
		
ContextT::ContextT(ContextT const &Context) : 
	LLVM(Context.LLVM), 
	Module(Context.Module),
	Block(Context.Block),
	Scope(Context.Scope)
	{}
	
ContextT ContextT::EnterScope(AtomT Scope)
{
	return ContextT(
		LLVM,
		Module,
		Block,
		Scope);
}

//================================================================================================================
// Interfaces
ValueT::~ValueT(void) {}
AtomT ValueT::Allocate(ContextT Context) { ERROR; }

AssignableT::~AssignableT(void) {}
void AssignableT::Assign(ContextT Context, AtomT Other) { ERROR; }

LLVMLoadableT::~LLVMLoadableT(void) {}

LLVMLoadableTypeT::~LLVMLoadableTypeT(void) {}

//================================================================================================================
// Basics
void UndefinedT::Assign(ContextT Context, AtomT Other) override
{
	auto Source = Other.As<ValueT>();
	if (!Source) ERROR;
	auto DestAtom = Source->Allocate(Context);
	auto Dest = DestAtom.As<AssignableT>();
	assert(Dest);
	Dest->Assign(Context, Other);
	Replace(DestAtom);
}

AtomT ImplementT::Allocate(ContextT Context) override
{
	auto Value = this->Value.As<ValueT>();
	if (!Value) ERROR;
	return Value->Allocate(Context);
}

void ImplementT::Simplify(ContextT Context) override
{
	Type->Simplify(Context);
	Value->Simplify(Context);
}

//================================================================================================================
// Primitives
PrimitiveTypeT::~PrimitiveTypeT(void) {}

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

void StringT::Assign(ContextT Context, AtomT Other) override
{
	GetType().As<StringTypeT>().Assign(Data, Other);
}

void StringTypeT::CheckType(bool Defined, AtomT Other)
{
	if (Static && Defined) ERROR;
	auto OtherType = Other.GetType().As<StringTypeT>();
	if (!OtherType) ERROR;
	if (OtherType.ID != ID) ERROR;
}
	
AtomT StringTypeT::Allocate(ContextT Context) override
{
	return {new StringT(*this)};
}

void StringTypeT::Assign(ContextT Context, std::string &Data, AtomT Other)
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

void StringTypeT::Assign(ContextT Context, llvm::Value *Target, AtomT Other) override { assert(false); }

template <typename DataT> void NumericT<DataT>::Assign(ContextT Context, AtomT Other) override
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

template <typename DataT> llvm::Value *NumericT<DataT>::GenerateLLVMLoad(ContextT Context) override
{
	return llvm::ConstantInt::get
		(
			llvm::IntegerType::get(Context.LLVM, sizeof(Data) * 8), 
			0, 
			false
		);
}

AtomT DynamicT::Allocate(ContextT Context)
{
	auto Type = GetType()->As<TypeT>();
	if (!Type) ERROR:
	return Type->Allocate();
}

void DynamicT::Assign(ContextT Context, AtomT Other)
{
	auto Type = GetType()->As<TypeT>();
	if (!Type) ERROR:
	Type->Assign(Context, Defined, Target, Other);
}

llvm::Value *DynamicT::GenerateLLVMLoad(ContextT Context)
{
	if (!Defined) ERROR;
	return new llvm::LoadInst(Target, "", Context.Block); 
}

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

bool NumericTypeT::IsSigned(void) const
{
	return DataType == DataTypeT::Int;
}

void NumericTypeT::CheckType(bool Defined, AtomT Other)
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

AtomT NumericTypeT::Allocate(ContextT Context) override
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

void NumericTypeT::Assign(ContextT Context, bool &Defined, llvm::Value *Target, AtomT Other) override
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

llvm::Type *NumericTypeT::GenerateLLVMType(ContextT Context) override
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

//================================================================================================================
// Groups
OptionalT<AtomT> GroupCollectionT::GetByKey(std::string const &Key)
{
	auto Found = Keys.find(Key);
	if (Found == Keys.end()) return {};
	return Found->second;
	//return Values[*Found];
}

void GroupCollectionT::Add(std::string const &Key, AtomT Value)
{
	auto Found = Keys.find(Key);
	if (Found != Keys.end()) ERROR;
	//auto Index = Values.size();
	//Values.push_back(Value);
	//Keys[Key] = Index;
	Keys[Key] = Value;
	return;
}

auto GroupCollectionT::begin(void) -> decltype(Keys.begin()) { return Keys.begin(); }
auto GroupCollectionT::end(void) -> decltype(Keys.end()) { return Keys.end(); }

void GroupT::Simplify(ContextT Context) override
{
	auto NewContext = Context.EnterScope(this);
	for (auto &Statement : Statements)
		Statement->Simplify(NewContext);
}

AtomT GroupT::Allocate(ContextT Context) override
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

void GroupT::Assign(ContextT Context, AtomT Other) override
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

AtomT GroupT::AccessElement(AtomT Key)
{
	auto KeyString = Key.As<StringT>();
	if (!KeyString) ERROR;
	auto Got = GetByKey(KeyString->Data);
	if (Got) return *Got;
	auto Out = new UndefinedT;
	Add(KeyString, Out);
	return Out;
}

void ElementT::Simplify(ContextT Context) override
{
	if (!Base) Base = Context.Scope;
	Base->Simplify(Context);
	Key->Simplify(Context);
	auto Group = Base.As<GroupT>();
	if (!Group) ERROR;
	Replace(Group->AccessElement(Key));
}

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

