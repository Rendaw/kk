#include "core.h"

namespace Core
{

constexpr TypeIDT DefaultTypeID = 0;

PositionBaseT::~PositionBaseT(void) {}

HardPositionT::HardPositionT(char const *File, char const *Function, int Line) : Position(::StringT() << "(HARD)" << File << "/" << Function << ":" << Line) {}

std::string HardPositionT::AsString(void) const { return Position; }

//================================================================================================================
// Core
void NucleusT::Replace(NucleusT *Replacement)
{
	auto OldAtoms = Atoms;
	for (auto Atom : OldAtoms)
		*Atom = Replacement;
}

NucleusT::NucleusT(PositionT const Position) : Position(Position) {}

NucleusT::~NucleusT(void) {}

void NucleusT::Simplify(ContextT Context) {}

//AtomT NucleusT::GetType(void) { return {nullptr}; }
AtomT NucleusT::GetType(ContextT Context) { ERROR; }

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
AtomT::AtomT(NucleusT *Nucleus) : Nucleus(nullptr) { Set(Nucleus); }
AtomT::AtomT(AtomT const &Other) : Nucleus(nullptr) { Set(Other.Nucleus); }
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
	AtomT Scope,
	PositionT Position) : 
	LLVM(LLVM), 
	Module(Module),
	Block(Block),
	Scope(Scope),
	Position(Position)
	{}
		
ContextT::ContextT(ContextT const &Context) : 
	LLVM(Context.LLVM), 
	Module(Context.Module),
	Block(Context.Block),
	Scope(Context.Scope),
	Position(Context.Position)
	{}
	
//================================================================================================================
// Interfaces
AssignableT::~AssignableT(void) {}

LLVMLoadableT::~LLVMLoadableT(void) {}

LLVMLoadableTypeT::~LLVMLoadableTypeT(void) {}

//================================================================================================================
// Basics
UndefinedT::UndefinedT(PositionT const Position) : NucleusT(Position) {}

void UndefinedT::Assign(ContextT Context, AtomT Other)
{
	Replace(Other);
}

ImplementT::ImplementT(PositionT const Position) : NucleusT(Position) {}

AtomT ImplementT::GetType(ContextT Context) { return Type; }

void ImplementT::Simplify(ContextT Context)
{
	Value->Simplify(Context);
	if (!Type) Type = Value->GetType(Context);
	else Type->Simplify(Context);
	auto Allocable = Type.As<TypeT>();
	if (!Allocable) ERROR;
	Replace(Allocable->Allocate(Context, Value));
}

//================================================================================================================
// Primitives
LLVMAssignableTypeT::~LLVMAssignableTypeT(void) {}

TypeT::~TypeT(void) {}

StringT::StringT(PositionT const Position) : NucleusT(Position) {}

AtomT StringT::GetType(ContextT Context)
{
	if (!Type) Type = new StringT(Position);
	return Type;
}

void StringT::Assign(ContextT Context, AtomT Other)
{
	GetType(Context).As<StringTypeT>()->Assign(Context, Data, Other);
}

StringTypeT::StringTypeT(PositionT const Position) : NucleusT(Position), ID(DefaultTypeID), Static(true) {}

void StringTypeT::CheckType(ContextT Context, AtomT Other)
{
	if (Static) ERROR;
	auto OtherType = Other->GetType(Context).As<StringTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
}
	
AtomT StringTypeT::Allocate(ContextT Context, AtomT Value)
{
	auto Out = new StringT(Context.Position);
	Out->Type = this;
	auto String = Value.As<StringT>();
	if (!String) ERROR;
	Out->Data = String->Data;
	return Out;
}

void StringTypeT::Assign(ContextT Context, std::string &Data, AtomT Other)
{
	CheckType(Context, Other);
	auto String = Other.As<StringT>();
	if (!String) ERROR;
	Data = String->Data;
}

template <typename DataT> NumericT<DataT>::NumericT(PositionT const Position) : NucleusT(Position) {}

NumericTypeT::DataTypeT GetDataType(ExplicitT<int32_t>) { return NumericTypeT::DataTypeT::Int; }
NumericTypeT::DataTypeT GetDataType(ExplicitT<uint32_t>) { return NumericTypeT::DataTypeT::UInt; }
NumericTypeT::DataTypeT GetDataType(ExplicitT<float>) { return NumericTypeT::DataTypeT::Float; }
NumericTypeT::DataTypeT GetDataType(ExplicitT<double>) { return NumericTypeT::DataTypeT::Double; }

template <typename DataT> AtomT NumericT<DataT>::GetType(ContextT Context)
{
	if (!Type) 
	{
		auto Temp = new NumericTypeT(Position);
		Temp->DataType = GetDataType(ExplicitT<DataT>());
		Type = Temp;
	}
	return Type;
}

template <typename DataT> void NumericT<DataT>::Assign(ContextT Context, AtomT Other)
{
	if (auto Number = Other.As<NumericT<DataT>>())
	{
		Data = Number->Data;
	}
	else if (auto Implementation = Other.As<ImplementT>())
	{
	}
	else ERROR;
}

template <> llvm::Value *NumericT<float>::GenerateLLVMLoad(ContextT Context)
{
	std::cout << "1 " << Data << std::endl;
	return llvm::ConstantFP::get
		(
			llvm::Type::getFloatTy(Context.LLVM),
			Data
		);
	//return llvm::ConstantFP::get(Context.LLVM, llvm::APFloat(Data));
}

template <> llvm::Value *NumericT<double>::GenerateLLVMLoad(ContextT Context)
{
	std::cout << "2 " << Data << std::endl;
	return llvm::ConstantFP::get
		(
			llvm::Type::getDoubleTy(Context.LLVM),
			Data
		);
	//return llvm::ConstantFP::get(Context.LLVM, llvm::APFloat(Data));
}

template <typename DataT> llvm::Value *NumericT<DataT>::GenerateLLVMLoad(ContextT Context)
{
	std::cout << "3 " << Data << std::endl;
	return llvm::ConstantInt::get
		(
			llvm::IntegerType::get(Context.LLVM, sizeof(Data) * 8), 
			Data, 
			false
		);
}

DynamicT::DynamicT(PositionT const Position) : NucleusT(Position), Target(nullptr) {}

AtomT DynamicT::GetType(ContextT Context) { return Type; }

void DynamicT::Assign(ContextT Context, AtomT Other)
{
	auto Type = GetType(Context).As<LLVMAssignableTypeT>();
	if (!Type) ERROR;
	Type->AssignLLVM(Context, Target, Other);
}

llvm::Value *DynamicT::GenerateLLVMLoad(ContextT Context)
{
	return new llvm::LoadInst(Target, "", Context.Block); 
}

llvm::Value *GenerateLLVMNumericConversion(llvm::BasicBlock *Block, llvm::Value *Source, llvm::Type *SourceType, bool SourceSigned, llvm::Type *DestType, bool DestSigned)
{
	if (SourceType->isIntegerTy())
	{
		if (SourceSigned)
		{
			if (DestType->isIntegerTy())
			{
				if (DestType->getIntegerBitWidth() < SourceType->getIntegerBitWidth())
				{
					return new llvm::TruncInst(Source, DestType, "", Block);
				}
				else if (DestType->getIntegerBitWidth() == SourceType->getIntegerBitWidth())
				{
					return Source;
				}
				else 
				{
					return new llvm::SExtInst(Source, DestType, "", Block);
				}
			}
			else
			{
				assert(DestType->isFloatTy() || DestType->isDoubleTy());
				return new llvm::SIToFPInst(Source, DestType, "", Block);
			}
		}
		else
		{
			if (DestType->isIntegerTy())
			{
				if (DestType->getIntegerBitWidth() < SourceType->getIntegerBitWidth())
				{
					return new llvm::TruncInst(Source, DestType, "", Block);
				}
				else if (DestType->getIntegerBitWidth() == SourceType->getIntegerBitWidth())
				{
					return Source;
				}
				else 
				{
					return new llvm::ZExtInst(Source, DestType, "", Block);
				}
			}
			else
			{
				assert(DestType->isFloatTy() || DestType->isDoubleTy());
				return new llvm::UIToFPInst(Source, DestType, "", Block);
			}
		}
	}
	else
	{
		if (SourceType->isFloatTy())
		{
			if (DestType->isIntegerTy())
			{
				if (DestSigned)
				{
					return new llvm::FPToSIInst(Source, DestType, "", Block);
				}
				else
				{
					return new llvm::FPToUIInst(Source, DestType, "", Block);
				}
			}
			else if (DestType->isFloatTy())
			{
				return Source;
			}
			else
			{
				assert(DestType->isDoubleTy());
				return new llvm::FPExtInst(Source, DestType, "", Block);
			}
		}
		else
		{
			assert(SourceType->isDoubleTy());
			if (DestType->isIntegerTy())
			{
				if (DestSigned)
				{
					return new llvm::FPToSIInst(Source, DestType, "", Block);
				}
				else
				{
					return new llvm::FPToUIInst(Source, DestType, "", Block);
				}
			}
			else if (DestType->isFloatTy())
			{
				return new llvm::FPTruncInst(Source, DestType, "", Block);
			}
			else
			{
				assert(DestType->isDoubleTy());
				return Source;
			}
		}
	}
}

NumericTypeT::NumericTypeT(PositionT const Position) : NucleusT(Position), ID(DefaultTypeID), Constant(true), Static(true), DataType(DataTypeT::Int) {}

bool NumericTypeT::IsSigned(void) const
{
	return DataType == DataTypeT::Int;
}

void NumericTypeT::CheckType(ContextT Context, AtomT Other)
{
	if (Static) ERROR;
	auto OtherType = Other->GetType(Context).As<NumericTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
	if (ID == DefaultTypeID)
	{
		if (Constant && !OtherType->Constant) ERROR;
		if (OtherType->DataType != DataType) ERROR;
	}
	else
	{
		assert(!Constant || OtherType->Constant);
		assert(DataType == OtherType->DataType);
	}
}

template <typename BaseT> AtomT NumericTypeConstantAssign(ContextT Context, NumericTypeT &Type, AtomT Value)
{
	auto Out = new NumericT<BaseT>(Context.Position);
	Out->Type = &Type;
	if (auto Number = Value.As<NumericT<int32_t>>()) Out->Data = Number->Data;
	else if (auto Number = Value.As<NumericT<uint32_t>>()) Out->Data = Number->Data;
	else if (auto Number = Value.As<NumericT<float>>()) Out->Data = Number->Data;
	else if (auto Number = Value.As<NumericT<double>>()) Out->Data = Number->Data;
	else ERROR;
	return Out;
}

AtomT NumericTypeT::Allocate(ContextT Context, AtomT Value)
{
	if (Constant)
	{
		switch (DataType)
		{
			case DataTypeT::Int: return NumericTypeConstantAssign<int32_t>(Context, *this, Value);
			case DataTypeT::UInt: return NumericTypeConstantAssign<uint32_t>(Context, *this, Value);
			case DataTypeT::Float: return NumericTypeConstantAssign<float>(Context, *this, Value);
			case DataTypeT::Double: return NumericTypeConstantAssign<double>(Context, *this, Value);
			default: assert(false); return nullptr;
		}
	}
	else 
	{
		auto Out = new DynamicT(Context.Position);
		Out->Type = this;
		
		auto Dest = new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.Block);
		Out->Target = Dest;
	
		llvm::Type *DestType = GenerateLLVMType(Context);
		bool DestSigned = IsSigned();
		
		auto Loadable = Value.As<LLVMLoadableT>();
		if (!Loadable) ERROR;
		auto Source = Loadable->GenerateLLVMLoad(Context);
		auto OtherType = Value->GetType(Context).As<NumericTypeT>();
		if (!OtherType) ERROR;
		llvm::Type *SourceType = OtherType->GenerateLLVMType(Context);
		bool SourceSigned = OtherType->IsSigned();
		
		Source = GenerateLLVMNumericConversion(
			Context.Block,
			Source,
			SourceType,
			SourceSigned,
			DestType,
			DestSigned);
		new llvm::StoreInst(Source, Dest, Context.Block);
		
		return Out;
	}
}

void NumericTypeT::AssignLLVM(ContextT Context, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	auto Loadable = Other.As<LLVMLoadableT>();
	if (!Loadable) ERROR;
	new llvm::StoreInst(Loadable->GenerateLLVMLoad(Context), Target, Context.Block);
}

llvm::Type *NumericTypeT::GenerateLLVMType(ContextT Context)
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
	assert(Found == Keys.end());
	////if (Found != Keys.end()) ERROR;
	//auto Index = Values.size();
	//Values.push_back(Value);
	//Keys[Key] = Index;
	Keys[Key] = Value;
}

auto GroupCollectionT::begin(void) -> decltype(Keys.begin()) { return Keys.begin(); }
auto GroupCollectionT::end(void) -> decltype(Keys.end()) { return Keys.end(); }

GroupT::GroupT(PositionT const Position) : NucleusT(Position) {}

void GroupT::Simplify(ContextT Context)
{
	Context.Scope = this;
	for (auto &Statement : Statements)
		Statement->Simplify(Context);
}

/*AtomT GroupT::Allocate(ContextT Context)
{
	Context.Scope = this;
	auto Out = new GroupT(Context.Position);
	for (auto &Pair : *this)
	{
		auto Value = Pair.second.As<ValueT>();
		if (!Value) ERROR;
		Out->Add(Pair.first, Value->Allocate(Context));
	}
	return {Out};
}*/

void GroupT::Assign(ContextT Context, AtomT Other)
{
	Context.Scope = this;
	auto Group = Other.As<GroupT>();
	if (!Group) ERROR;
	for (auto &Pair : *this)
	{
		auto Dest = Pair.second.As<AssignableT>();
		if (!Dest) ERROR;
		auto Source = GetByKey(Pair.first);
		if (!Source) ERROR;
		Dest->Assign(Context, *Source);
	}
}

AtomT GroupT::AccessElement(ContextT Context, AtomT Key)
{
	auto KeyString = Key.As<StringT>();
	if (!KeyString) ERROR;
	auto Got = GetByKey(KeyString->Data);
	if (Got) return *Got;
	auto Out = new UndefinedT(Context.Position);
	Add(KeyString->Data, Out);
	return Out;
}

ElementT::ElementT(PositionT const Position) : NucleusT(Position) {}

void ElementT::Simplify(ContextT Context)
{
	if (!Base) Base = Context.Scope;
	else Base->Simplify(Context);
	Key->Simplify(Context);
	auto Group = Base.As<GroupT>();
	if (!Group) ERROR;
	Replace(Group->AccessElement(Context, Key));
}

//================================================================================================================
// Statements
AssignmentT::AssignmentT(PositionT const Position) : NucleusT(Position) {}

void AssignmentT::Simplify(ContextT Context)
{
	Context.Position = Position;
	Left->Simplify(Context);
	Right->Simplify(Context);
	auto Assignable = Left.As<AssignableT>();
	if (!Assignable) ERROR;
	Assignable->Assign(Context, Right);
}

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
			// If return value is struct, - Result = DynamicT(Allocated)
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
/*FunctionT::FunctionT(PositionT const Position) : NucleusT(Position) {}

void FunctionT::Simplify(ContextT Context) override
{
	Type->Simplify(Context);
}

AtomT FunctionT::Call(ContextT Context, AtomT Input)
{
	
	////
	auto Result = Function.Refine(Input);
	if (auto DynamicFunction = Result.As<DynamicFunctionT>())
	{
		// If return value is struct, allocate + set as first of DynamicInput
		// Pull out linearization of dynamic elements from Input -> DynamicInput
		// Result = LLVM call create
		// If return value is struct, - Result = DynamicT(Allocated)
	}
	return Result;
}

CallT::CallT(PositionT const Position) : NucleusT(Position) { }

void CallT::Simplify(ContextT Context)
{
	Function->Simplify(Context);
	Input->Simplify(Context);
	auto Function = this->Function.As<FunctionT>();
	if (!Function) ERROR;
	Replace(Function->Call(Context, Input));
}*/

}

