#include "core.h"

namespace Core
{

constexpr TypeIDT DefaultTypeID = 0;

PositionBaseT::~PositionBaseT(void) {}

HardPositionT::HardPositionT(char const *File, char const *Function, int Line) : Position(::StringT() << "(HARD)" << File << "/" << Function << ":" << Line) {}

std::string HardPositionT::AsString(void) const { return Position; }


llvm::Value *GetElementPointer(llvm::Value *Base, int32_t Index, llvm::BasicBlock *Block)
{
	std::vector<llvm::Value *> Indices;
	Indices.push_back(llvm::ConstantInt::get
	(
		llvm::IntegerType::get(Context.LLVM, 32), 
		0, 
		false
	));
	Indices.push_back(llvm::ConstantInt::get
	(
		llvm::IntegerType::get(Context.LLVM, 32), 
		Index, 
		false
	));
	return llvm::GetElementPtrInst::Create(Base, Indices, "", Block);
}

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

AtomT NucleusT::Clone(void) { assert(false); return {}; }

AtomT NucleusT::GetType(ContextT Context) { ERROR; }

void NucleusT::Simplify(ContextT Context) {}

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
AtomT::operator bool(void) const { return Nucleus; }
bool AtomT::operator !(void) const { return !Nucleus; }

NucleusT *AtomT::operator ->(void) { return Nucleus; }

ContextT::ContextT(
	llvm::LLVMContext &LLVM, 
	llvm::Module *Module, 
	llvm::BasicBlock *Block, 
	AtomT Scope,
	PositionT Position,
	bool IsConstant) : 
	LLVM(LLVM), 
	Module(Module),
	Block(Block),
	Scope(Scope),
	Position(Position),
	IsConstant(IsConstant)
	{}
		
ContextT::ContextT(ContextT const &Context) : 
	LLVM(Context.LLVM), 
	Module(Context.Module),
	Block(Context.Block),
	Scope(Context.Scope),
	Position(Context.Position),
	IsConstant(Context.IsConstant)
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

AtomT ImplementT::Clone(void)
{
	auto Out = new ImplementT(Position);
	if (Type)
		Out->Type = Type->Clone();
	if (Value)
		Out->Value = Value->Clone();
	return Out;
}

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

StringT::StringT(PositionT const Position) : NucleusT(Position), Initialized(false) {}

AtomT StringT::Clone(void)
{
	auto Out = new StringT(Position);
	Out->Initialized = Initialized;
	Out->Type = Type;
	Out->Data = Data;
	return Out;
}

AtomT StringT::GetType(ContextT Context)
{
	if (!Type) Type = new StringT(Position);
	return Type;
}

void StringT::Assign(ContextT Context, AtomT Other)
{
	GetType(Context).As<StringTypeT>()->Assign(Context, Initialized, Data, Other);
}

StringTypeT::StringTypeT(PositionT const Position) : NucleusT(Position), ID(DefaultTypeID), Static(true) {}

AtomT StringTypeT::Clone(void)
{
	auto Out = new StringTypeT(Position);
	Out->ID = ID;
	Out->Static = Static;
	return Out;
}

bool StringTypeT::IsDynamic(void)
{
	return false;
}

void StringTypeT::CheckType(ContextT Context, AtomT Other)
{
	auto OtherType = Other->GetType(Context).As<StringTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
}
	
AtomT StringTypeT::Allocate(ContextT Context, AtomT Value)
{
	auto Out = new StringT(Context.Position);
	Out->Type = this;
	if (Value)
	{
		if (auto String = Value.As<StringT>())
		{
			if (!String->Initialized) ERROR;
			Out->Data = String->Data;
		}
		else ERROR;
		Out->Initialized = true;
	}
	return Out;
}

void StringTypeT::Assign(ContextT Context, bool &Initialized, std::string &Data, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized && Static) ERROR;
	auto String = Other.As<StringT>();
	if (!String) ERROR;
	if (!String->Initialized) ERROR;
	Data = String->Data;
	Initialized = true;
}

template <typename DataT> NumericT<DataT>::NumericT(PositionT const Position) : NucleusT(Position), Initialized(false) {}

template <typename DataT> AtomT NumericT<DataT>::Clone(void)
{
	auto Out = new NumericT<DataT>(Position);
	Out->Initialized = Initialized;
	Out->Type = Type;
	Out->Data = Data;
	return Out;
}

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
	auto Type = GetType(Context).template As<NumericTypeT>();
	Type->CheckType(Context, Other);
	if (Type->Static && Initialized) ERROR;
	auto Number = Other.As<NumericT<DataT>>();
	if (!Number) ERROR;
	if (!Number->Initialized) ERROR;
	Data = Number->Data;
}

template <> llvm::Value *NumericT<float>::GenerateLLVMLoad(ContextT Context)
{
	if (!Initialized) ERROR;
	return llvm::ConstantFP::get
		(
			llvm::Type::getFloatTy(Context.LLVM),
			Data
		);
}

template <> llvm::Value *NumericT<double>::GenerateLLVMLoad(ContextT Context)
{
	if (!Initialized) ERROR;
	return llvm::ConstantFP::get
		(
			llvm::Type::getDoubleTy(Context.LLVM),
			Data
		);
}

template <typename DataT> llvm::Value *NumericT<DataT>::GenerateLLVMLoad(ContextT Context)
{
	if (!Initialized) ERROR;
	return llvm::ConstantInt::get
		(
			llvm::IntegerType::get(Context.LLVM, sizeof(Data) * 8), 
			Data, 
			false
		);
}

DynamicT::DynamicT(PositionT const Position) : NucleusT(Position), Initialized(false), Target(nullptr) {}

AtomT DynamicT::GetType(ContextT Context) { return Type; }

llvm::Value *DynamicT::GetTarget(ContextT Context)
{
	if (!Target)
	{
		Assert(StructTarget);
		Target = GetElementPointer(StructTarget->Struct, StructTarget->Index, Context.Block);
	}
	return Target;
}

void DynamicT::Assign(ContextT Context, AtomT Other)
{
	auto Type = GetType(Context).As<LLVMAssignableTypeT>();
	if (!Type) ERROR;
	Type->AssignLLVM(Context, Initialized, GetTarget(Context), Other);
}

llvm::Value *DynamicT::GenerateLLVMLoad(ContextT Context)
{
	if (!Initialized) ERROR;
	return new llvm::LoadInst(GetTarget(Context), "", Context.Block); 
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
				AssertOr(DestType->isFloatTy(), DestType->isDoubleTy());
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
				AssertOr(DestType->isFloatTy(), DestType->isDoubleTy());
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
				Assert(DestType->isDoubleTy());
				return new llvm::FPExtInst(Source, DestType, "", Block);
			}
		}
		else
		{
			Assert(SourceType->isDoubleTy());
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
				Assert(DestType->isDoubleTy());
				return Source;
			}
		}
	}
}

NumericTypeT::NumericTypeT(PositionT const Position) : NucleusT(Position), ID(DefaultTypeID), Constant(true), Static(true), DataType(DataTypeT::Int) {}

AtomT NumericTypeT::Clone(void)
{
	auto Out = new NumericTypeT(Position);
	Out->ID = ID;
	Out->Constant = Constant;
	Out->Static = Static;
	return Out;
}

bool NumericTypeT::IsDynamic(void) 
{
	return !Constant;
}

bool NumericTypeT::IsSigned(void) const
{
	return DataType == DataTypeT::Int;
}

void NumericTypeT::CheckType(ContextT Context, AtomT Other)
{
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
		AssertOr(!Constant, OtherType->Constant);
		AssertE((int)DataType, (int)OtherType->DataType);
	}
}

template <typename BaseT> AtomT NumericTypeConstantAssign(ContextT Context, NumericTypeT &Type, AtomT Value)
{
	auto Out = new NumericT<BaseT>(Context.Position);
	Out->Type = &Type;
	if (Value)
	{
		if (auto Number = Value.As<NumericT<int32_t>>()) { if (!Number->Initialized) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<uint32_t>>()) { if (!Number->Initialized) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<float>>()) { if (!Number->Initialized) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<double>>()) { if (!Number->Initialized) ERROR; Out->Data = Number->Data; }
		else ERROR;
		Out->Initialized = true;
	}
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
		Context.IsConstant = false;
		
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
		
		Out->Initialized = true;
		return Out;
	}
}

void NumericTypeT::AssignLLVM(ContextT Context, bool &Initialized, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized && Static) ERROR;
	Initialized = true;
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
}

void GroupCollectionT::Add(std::string const &Key, AtomT Value)
{
	auto Found = Keys.find(Key);
	Assert(Found == Keys.end());
	Keys[Key] = Value;
}

auto GroupCollectionT::begin(void) -> decltype(Keys.begin()) { return Keys.begin(); }
auto GroupCollectionT::end(void) -> decltype(Keys.end()) { return Keys.end(); }

GroupT::GroupT(PositionT const Position) : NucleusT(Position) {}

AtomT GroupT::Clone(void)
{
	auto Out = new GroupT(Position);
	for (auto &Statement : Statements)
		Out->Statements.push_back(Statement->Clone());
	for (auto &Pair : *this)
		Out->Add(Pair.first, Pair.second->Clone());
	return Out;
}

void GroupT::Simplify(ContextT Context)
{
	Context.Scope = this;
	for (auto &Statement : Statements)
		Statement->Simplify(Context);
}

void GroupT::Assign(ContextT Context, AtomT Other)
{
	Context.Scope = this;
	auto Group = Other.As<GroupT>();
	if (!Group) ERROR;
	for (auto &Pair : *this)
	{
		auto Dest = Pair.second.As<AssignableT>();
		if (!Dest) ERROR;
		auto Source = Group->GetByKey(Pair.first);
		if (!Source) ERROR;
		Dest->Assign(Context, *Source);
	}
}

AtomT GroupT::AccessElement(ContextT Context, std::string const &Key)
{
	auto Got = GetByKey(Key);
	if (Got) return *Got;
	auto Out = new UndefinedT(Context.Position);
	Add(Key, Out);
	return Out;
}

AtomT GroupT::AccessElement(ContextT Context, AtomT Key)
{
	auto KeyString = Key.As<StringT>();
	if (!KeyString) ERROR;
	return AccessElement(Context, KeyString->Data);
}

BlockT::BlockT(PositionT const Position) : NucleusT(Position) {}

AtomT BlockT::Clone(void) 
{
	auto Out = new BlockT(Position);
	for (auto &Statement : Statements)
		Out->Statements.push_back(Statement->Clone());
	return Out;
}

void BlockT::Simplify(ContextT Context) {}

AtomT BlockT::CloneGroup(void)
{
	auto Out = new GroupT(Position);
	for (auto &Statement : Statements)
		Out->Statements.push_back(Statement->Clone());
	return Out;
}

ElementT::ElementT(PositionT const Position) : NucleusT(Position) {}

AtomT ElementT::Clone(void)
{
	auto Out = new ElementT(Position);
	if (Base)
		Out->Base = Base->Clone();
	if (Key)
		Out->Key = Key->Clone();
	return Out;
}

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
// Type manipulations
AsDynamicTypeT::AsDynamicTypeT(PositionT const Position) : NucleusT(Position) {}

AtomT AsDynamicTypeT::Clone(void)
{
	auto Out = new AsDynamicTypeT(Position);
	if (Type)
		Out->Type = Type->Clone();
	return Out;
}

void AsDynamicTypeT::Simplify(ContextT Context) 
{
	Type->Simplify(Context);
	auto NewType = Type->Clone();
	if (auto Numeric = NewType.As<NumericTypeT>())
	{
		Numeric->Constant = false;
	}
	else if (auto FunctionType = NewType.As<FunctionTypeT>())
	{
		FunctionType->Constant = false;
	}
	else ERROR;
	Replace(NewType);
}

//================================================================================================================
// Statements
AssignmentT::AssignmentT(PositionT const Position) : NucleusT(Position) {}

AtomT AssignmentT::Clone(void)
{
	auto Out = new AssignmentT(Position);
	Out->Left = Left->Clone();
	Out->Right = Right->Clone();
	return Out;
}

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

FunctionTypeT::FunctionTypeT(PositionT const Position) : NucleusT(Position), ID(0), Constant(true), Static(true) {}

AtomT FunctionTypeT::Clone(void)
{
	auto Out = new FunctionTypeT(Position);
	Out->ID = ID;
	Out->Constant = Constant;
	Out->Static = Static;
	Out->Signature = Signature->Clone();
	return Out;
}

void FunctionTypeT::Simplify(ContextT Context)
{
	Signature->Simplify(Context);
}

AtomT FunctionTypeT::Allocate(ContextT Context, AtomT Value) 
{
	auto Function = new FunctionT(Context.Position);
	Function->Type = this;
	if (Value) Function->Body = Value;
	if (Constant)
	{
		return Function;
	}
	else
	{
		auto Dynamic = new DynamicT(Context.Position);
		Dynamic->Type = this;
		Dynamic->Target = new llvm::AllocaInst(llvm::PointerType::getUnqual(GenerateLLVMType(Context)), "", Context.Block);
		if (Value) Dynamic->Assign(Context, Function);
		return Dynamic;
	}
}

bool FunctionTypeT::IsDynamic(void)
{
	return !Constant;
}

void FunctionTypeT::CheckType(ContextT Context, AtomT Other)
{
	AssertNE(ID, 0);
	auto OtherType = Other->GetType(Context).As<FunctionTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
}

llvm::Type *FunctionTypeT::GenerateLLVMType(ContextT Context)
{
	auto Result = ProcessFunction(Context, GenerateLLVMTypeParamsT{});
	return Result.Get<GenerateLLVMTypeResultsT>().Type;
}
	
void FunctionTypeT::AssignLLVM(ContextT Context, bool &Initialized, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized && Static) ERROR;
	if (auto Function = Other.As<FunctionT>())
	{
		auto Result = ProcessFunction(Context, GenerateLLVMLoadParamsT{*Function});
		new llvm::StoreInst(Result.Get<GenerateLLVMLoadResultsT>().Value, Target, Context.Block);
	}
	else if (auto Dynamic = Other.As<DynamicT>())
	{
		new llvm::StoreInst(Dynamic->GenerateLLVMLoad(Context), Target, Context.Block);
	}
	else ERROR;
	Initialized = true;
}

AtomT FunctionTypeT::Call(ContextT Context, AtomT Function, AtomT Input)
{
	auto Result = ProcessFunction(Context, CallParamsT{Function, Input});
	return Result.Get<CallResultsT>().Result;
}

std::vector<uint8_t> GetLLVMArgID(ExplicitT<DynamicT>)
{
	return {0};
}

std::vector<uint8_t> GetLLVMArgID(ExplicitT<GroupT>)
{
	return {1};
}

std::vector<uint8_t> GetLLVMArgID(AtomT const &Value)
{
	if (auto Dynamic = Value.As<DynamicT>())
	{
		return GetLLVMArgID(ExplicitT<DynamicT>());
	}
	else if (auto Group = Value.As<GroupT>())
	{
		return GetLLVMArgID(ExplicitT<GroupT>());
	}
	else if (auto String = Value.As<StringT>())
	{
		std::vector<uint8_t> Out;
		Out.resize(1 + String->Data.size());
		Out[0] = 2;
		std::copy(String->Data.begin(), String->Data.end(), Out.begin() + 1);
		return Out;
	}
	else if (auto Number = Value.As<NumericT<int32_t>>())
	{
		std::vector<uint8_t> Out;
		Out.resize(1 + sizeof(Number->Data));
		Out[0] = 3;
		*reinterpret_cast<decltype(Number->Data) *>(&Out[1]) = Number->Data;
		return Out;
	}
	else if (auto Number = Value.As<NumericT<uint32_t>>())
	{
		std::vector<uint8_t> Out;
		Out.resize(1 + sizeof(Number->Data));
		Out[0] = 4;
		*reinterpret_cast<decltype(Number->Data) *>(&Out[1]) = Number->Data;
		return Out;
	}
	else if (auto Number = Value.As<NumericT<float>>())
	{
		std::vector<uint8_t> Out;
		Out.resize(1 + sizeof(Number->Data));
		Out[0] = 5;
		*reinterpret_cast<decltype(Number->Data) *>(&Out[1]) = Number->Data;
		return Out;
	}
	else if (auto Number = Value.As<NumericT<double>>())
	{
		std::vector<uint8_t> Out;
		Out.resize(1 + sizeof(Number->Data));
		Out[0] = 6;
		*reinterpret_cast<decltype(Number->Data) *>(&Out[1]) = Number->Data;
		return Out;
	}
	else assert(false);
	return {};
}

constexpr auto FunctionInputKey = "input";
constexpr auto FunctionOutputKey = "output";
FunctionTypeT::ProcessFunctionResultT FunctionTypeT::ProcessFunction(ContextT Context, ProcessFunctionParamT Param)
{
	FunctionTreeT<FunctionT::CachedLLVMFunctionT> *FunctionTree = nullptr;
	AtomT CallInput;
	
	AtomT Body;

	llvm::Type *LLVMResultStructType = nullptr;
	llvm::Value *LLVMFunction = nullptr;
	
	if (Param.Is<GenerateLLVMTypeParamsT>())
	{
		// NOP
	}
	else if (Param.Is<GenerateLLVMLoadParamsT>())
	{
		auto &Params = Param.Get<GenerateLLVMLoadParamsT>();
		Body = Params.Function->Body;
		FunctionTree = &Params.Function->InstanceTree;
	}
	else if (Param.Is<CallParamsT>())
	{
		auto &Params = Param.Get<CallParamsT>();
		if (auto Function = Params.Function.As<FunctionT>())
		{
			Body = Function->Body;
			FunctionTree = &Function->InstanceTree;
		}
		else if (auto Dynamic = Params.Function.As<DynamicT>())
		{
			LLVMFunction = Dynamic->GenerateLLVMLoad(Context);
		}
		else ERROR;
		CallInput = Params.Input;
	}
	else assert(false);
	
	auto TypeLookup = TypeTree.StartLookup();
	OptionalT<FunctionTreeT<FunctionT::CachedLLVMFunctionT>::LookupT> FunctionLookup;
	if (FunctionTree) FunctionLookup = FunctionTree->StartLookup();
	
	auto FunctionContext = Context;
	FunctionContext.IsConstant = true;
	
	OptionalT<BlockT *> BlockBody;
	OptionalT<GroupT *> BodyGroup;
	if (Body)
	{
		BlockBody = Body.As<BlockT>();
		if (!*BlockBody) ERROR;
		Body = BlockBody->CloneGroup();
		BodyGroup = Body.As<GroupT>();
		Assert(*BodyGroup);
	}
	
	auto Signature = this->Signature.As<GroupT>();
	if (!Signature) ERROR;
	auto InputType = Signature->GetByKey(FunctionInputKey);
	auto OutputType = Signature->GetByKey(FunctionOutputKey);

	std::vector<llvm::Type *> LLVMArgTypes;
	std::vector<llvm::Type *> LLVMReturnTypes;
	
	// OUTSIDE
	// argument CallInput
	AtomT CallOutput;
	if (Param.Is<CallParamsT>()) 
	{
		CallOutput = new UndefinedT(Context.Position);
	}
	std::vector<llvm::Value *> DynamicCallInput; // Values passed to llvm::callinst
	std::vector<DynamicT *> DynamicCallOutput; // Dynamics parsed from result of llvm::callinst
	
	// INSIDE
	AtomT BodyInput;
	AtomT BodyOutput;
	if (Body) 
	{
		BodyInput = (*BodyGroup)->AccessElement(Context, FunctionInputKey);
		BodyOutput = (*BodyGroup)->AccessElement(Context, FunctionOutputKey);
	}
	std::vector<DynamicT *> DynamicBodyOutput;
	std::vector<DynamicT *> DynamicBodyInput;
	
	if (OutputType)
	{
		std::function<void(AtomT Type, AtomT Body, AtomT Call)> ClassifyOutput;
		ClassifyOutput = [&](AtomT Type, AtomT Body, AtomT Call)
		{
			OptionalT<AssignableT *> BodyAssignable;
			if (Body)
			{
				BodyAssignable = Body.As<AssignableT>();
				Assert(*BodyAssignable);
			}
			
			OptionalT<AssignableT *> CallAssignable;
			if (Param.Is<CallParamsT>())
			{
				CallAssignable = Call.As<AssignableT>();
				Assert(*CallAssignable);
			}
			
			if (auto Group = Type.As<GroupT>())
			{
				OptionalT<GroupT *> BodyGroup;
				if (Body)
				{
					BodyGroup = new GroupT(Context.Position);
					(*BodyAssignable)->Assign(Context, *BodyGroup);
				}
				
				OptionalT<GroupT *> CallGroup;
				if (Param.Is<CallParamsT>())
				{
					CallGroup = new GroupT(Context.Position);
					(*CallAssignable)->Assign(Context, *CallGroup);
				}
				
				for (auto &TypePair : **Group)
				{
					ClassifyOutput(
						TypePair.second, 
						Body ? (*BodyGroup)->AccessElement(Context, TypePair.first) : AtomT{}, 
						Param.Is<CallParamsT>() ? (*CallGroup)->AccessElement(Context, TypePair.first) : AtomT{});
				}
			}
			else if (auto SimpleType = Type.As<TypeT>())
			{
				if (SimpleType->IsDynamic())
				{
					FunctionContext.IsConstant = false;
					
					auto LLVMType = Type.As<LLVMLoadableTypeT>();
					Assert(LLVMType);
					LLVMReturnTypes.push_back(LLVMType->GenerateLLVMType(Context));
					
					if (Body)
					{
						auto BodyDynamic = new DynamicT(Context.Position);
						BodyDynamic->Type = Type;
						DynamicBodyOutput.push_back(BodyDynamic);
						BodyAssignable->Assign(Context, BodyDynamic);
					}
					
					if (Param.Is<CallParamsT>())
					{
						auto CallDynamic = new DynamicT(Context.Position);
						CallDynamic->Type = Type;
						DynamicCallOutput.push_back(CallDynamic);
						CallAssignable->Assign(Context, CallDynamic);
					}
				}
				else
				{
					if (Param.Is<GenerateLLVMTypeParamsT>() || Param.Is<GenerateLLVMLoadParamsT>())
						ERROR;
					Assert(Body);
					Assert(Param.Is<CallParamsT>());
					auto ConstantOutput = SimpleType->Allocate(Context, {});
					BodyAssignable->Assign(Context, ConstantOutput);
					CallAssignable->Assign(Context, ConstantOutput);
				}
			}
			else ERROR;
		};
		ClassifyOutput(*OutputType, BodyOutput, CallOutput);
		
		if (LLVMReturnTypes.size() == 1)
		{
			Assert(DynamicBodyOutput.size() == 1);
			DynamicBodyOutput[0]->Target = new llvm::AllocaInst(LLVMReturnTypes[0], "", Context.Block);
		}
		else if (LLVMReturnTypes.size() > 1)
		{
			if (TypeLookup) 
			{
				Assert(TypeLookup->ResultStructType);
				LLVMResultStructType = *TypeLookup->ResultStructType;
			}
			else
			{
				LLVMResultStructType = llvm::StructType::create(LLVMReturnTypes);
			}
			LLVMArgTypes.push_back(llvm::PointerType::getUnqual(LLVMResultStructType));
			
			if (Param.Is<CallParamsT>())
			{
				auto LLVMResultStruct = new llvm::AllocaInst(LLVMResultStructType, "", Context.Block);
				DynamicCallInput.push_back(LLVMResultStruct);
				
				size_t StructIndex = 0;
				for (auto &Dynamic : DynamicCallOutput)
					Dynamic->StructTarget = DynamicT::StructTargetT{LLVMResultStruct, StructIndex++};
			}
		}
	}
	
	if (InputType)
	{
		std::function<void(AtomT Type, AtomT Call, AtomT Body)> ClassifyInput;
		ClassifyInput = [&](AtomT Type, AtomT Call, AtomT Body)
		{
			if (Param.Is<CallParamsT>())
			{
				TypeLookup.Enter(GetLLVMArgID(Call));
				if (FunctionLookup)
					FunctionLookup->Enter(GetLLVMArgID(Call));
			}
			
			OptionalT<AssignableT *> BodyAssignable;
			if (Body)
			{
				auto OptBodyAssignable = Body.As<AssignableT>();
				Assert(OptBodyAssignable);
				BodyAssignable = *OptBodyAssignable;
			}
			
			if (auto Group = Type.As<GroupT>())
			{
				OptionalT<GroupT *> CallGroup;
				if (Param.Is<CallParamsT>())
				{
					auto OptCallGroup = Call.As<GroupT>();
					if (!OptCallGroup) ERROR;
					CallGroup = *OptCallGroup;
				}
				else
				{
					TypeLookup.Enter(GetLLVMArgID(ExplicitT<GroupT>()));
					if (FunctionLookup)
						FunctionLookup->Enter(GetLLVMArgID(ExplicitT<GroupT>()));
				}
				
				OptionalT<GroupT *> BodyGroup;
				if (Body)
				{
					BodyGroup = new GroupT(Context.Position);
				}
				
				for (auto &TypePair : **Group)
				{
					AtomT SubCall;
					if (Param.Is<CallParamsT>())
					{
						auto OptSubCall = (*CallGroup)->GetByKey(TypePair.first);
						if (!OptSubCall) ERROR;
						SubCall = *OptSubCall;
					}
					
					AtomT SubBody;
					if (Body)
					{
						SubBody = (*BodyGroup)->AccessElement(Context, TypePair.first);
					}
					
					ClassifyInput(TypePair.second, SubCall, SubBody);
				}
				
				if (Body)
				{
					(*BodyAssignable)->Assign(Context, *BodyGroup);
				}
			}
			else if (auto SimpleType = Type.As<TypeT>())
			{
				if (Param.Is<CallParamsT>())
					SimpleType->CheckType(Context, Call);
				
				if (SimpleType->IsDynamic())
				{
					FunctionContext.IsConstant = false;
					
					auto LLVMType = Type.As<LLVMLoadableTypeT>();
					Assert(LLVMType);
					LLVMArgTypes.push_back(LLVMType->GenerateLLVMType(Context));
					
					if (Param.Is<CallParamsT>())
					{
						auto CallValue = Call.As<LLVMLoadableT>();
						if (!CallValue) ERROR;
						DynamicCallInput.push_back(CallValue->GenerateLLVMLoad(Context));
					}
					else
					{
						TypeLookup.Enter(GetLLVMArgID(ExplicitT<DynamicT>()));
						if (FunctionLookup)
							FunctionLookup->Enter(GetLLVMArgID(ExplicitT<DynamicT>()));
					}
					
					if (Body)
					{
						auto BodyDynamic = new DynamicT(Context.Position);
						DynamicBodyInput.push_back(BodyDynamic);
						BodyAssignable->Assign(Context, BodyDynamic);
					}
				}
				else
				{
					if (Param.Is<GenerateLLVMTypeParamsT>() || Param.Is<GenerateLLVMLoadParamsT>())
						ERROR;
					Assert(Body);
					Assert(Param.Is<CallParamsT>());
					auto BodyAtom = SimpleType->Allocate(Context, {});
					auto BodyValue = BodyAtom.As<AssignableT>();
					BodyValue->Assign(Context, Call);
					BodyAssignable->Assign(Context, BodyAtom);
				}
			}
			else ERROR;
		};
		ClassifyInput(*InputType, CallInput, BodyInput);
	}
	
	llvm::FunctionType *LLVMFunctionType;
	if (TypeLookup)
	{
		LLVMFunctionType = TypeLookup->FunctionType;
	}
	else
	{
		llvm::Type *LLVMReturnType;
		if (LLVMReturnTypes.size() == 1) LLVMReturnType = LLVMReturnTypes[0];
		else LLVMReturnType = llvm::Type::getVoidTy(Context.LLVM);
		
		LLVMFunctionType = llvm::FunctionType::get(LLVMReturnType, LLVMArgTypes, false);
		
		TypeLookup->FunctionType = LLVMFunctionType;
		TypeLookup->ResultStructType = LLVMResultStructType;
	}
	
	if (Param.Is<GenerateLLVMTypeParamsT>()) return GenerateLLVMTypeResultsT{LLVMFunctionType}; // NOTE Return
	
	if (LLVMFunction)
	{
		// NOP
	}
	else if (FunctionLookup && *FunctionLookup)
	{
		LLVMFunction = (*FunctionLookup)->Function;
		FunctionContext.IsConstant = (*FunctionLookup)->IsConstant;
	}
	else
	{
		auto SpecificLLVMFunction = llvm::Function::Create(LLVMFunctionType, llvm::Function::PrivateLinkage, "", Context.Module);
		LLVMFunction = SpecificLLVMFunction;

		if (LLVMReturnTypes.size() > 1)
		{
			SpecificLLVMFunction->addAttribute(1, llvm::Attribute::StructRet);
		}
		
		// Add to lookup here for recursion and mutual-recursion purposes
		Assert(FunctionLookup);
		(*FunctionLookup)->Function = LLVMFunction;
		(*FunctionLookup)->IsConstant = FunctionContext.IsConstant;
		
		auto Block = llvm::BasicBlock::Create(Context.LLVM, "entrypoint", SpecificLLVMFunction);
		
		{
			size_t Index = 0, InputIndex = 0;
			for (auto &Argument : SpecificLLVMFunction->getArgumentList())
			{
				if ((Index == 0) && (LLVMReturnTypes.size() > 1))
				{
					size_t StructIndex = 0;
					AssertE(DynamicBodyOutput.size(), LLVMReturnTypes.size());
					for (auto &Dynamic : DynamicBodyOutput)
						Dynamic->StructTarget = DynamicT::StructTargetT{&Argument, StructIndex++};
				}
				else
				{
					AssertLT(InputIndex, DynamicBodyInput.size());
					DynamicBodyInput[InputIndex++]->Target = &Argument;
				}
				++Index;
			}
			AssertE(InputIndex, DynamicBodyInput.size());
		}
		
		FunctionContext.Block = Block;
		(*BodyGroup)->Simplify(FunctionContext);
		
		if (LLVMReturnTypes.size() == 1)
			llvm::ReturnInst::Create(Context.LLVM, DynamicBodyOutput[0]->GenerateLLVMLoad(Context), Block);
		else llvm::ReturnInst::Create(Context.LLVM, Block);
	}

	if (Param.Is<GenerateLLVMLoadParamsT>()) return GenerateLLVMLoadResultsT{LLVMFunction}; // NOTE Return
	
	if (!FunctionContext.IsConstant)
	{
		Context.IsConstant = false;
		auto Result = llvm::CallInst::Create(LLVMFunction, DynamicCallInput, "", Context.Block);
		
		if (LLVMReturnTypes.size() == 1)
		{
			AssertE(DynamicCallOutput.size(), 1u);
			DynamicCallOutput[0]->Target = Result;
		}
	}
	
	return CallResultsT{BodyOutput}; // NOTE Return
}

FunctionT::FunctionT(PositionT const Position) : NucleusT(Position) {}

AtomT FunctionT::GetType(ContextT Context) { return Type; }

void FunctionT::Simplify(ContextT Context)
{
	Type->Simplify(Context);
}

AtomT FunctionT::Call(ContextT Context, AtomT Input)
{
	return GetType(Context).As<FunctionTypeT>()->Call(Context, Body, Input);
}

CallT::CallT(PositionT const Position) : NucleusT(Position) { }

AtomT CallT::Clone(void)
{
	auto Out = new CallT(Position);
	Out->Function = Function->Clone();
	if (Input) 
		Out->Input = Input->Clone();
	return Out;
}

void CallT::Simplify(ContextT Context)
{
	Function->Simplify(Context);
	Input->Simplify(Context);
	AtomT Type;
	if (auto Function = this->Function.As<FunctionT>())
		Type = Function->GetType(Context);
	else if (auto Dynamic = this->Function.As<DynamicT>())
	{
		if (auto FunctionType = Dynamic->Type.As<FunctionTypeT>())
			Type = *FunctionType;
		else ERROR;
	}
	else ERROR;
	auto FunctionType = Type.As<FunctionTypeT>();
	if (!FunctionType) ERROR;
	Replace(FunctionType->Call(Context, Function, Input));
}

ModuleT::ModuleT(PositionT const Position) : NucleusT(Position), Entry(false) {}

void ModuleT::Simplify(ContextT Context)
{
	if (!Top) ERROR;
	auto TopGroup = Top.As<GroupT>();
	
	auto &LLVM = Context.LLVM;
	if (Name.empty()) ERROR;
	auto Module = new llvm::Module(Name.c_str(), LLVM);
	
	NumericTypeT *ReturnType = nullptr;
	
	llvm::FunctionType *FunctionType = nullptr;
	if (Entry)
	{
		ReturnType = new NumericTypeT(Context.Position);
		ReturnType->Constant = false;
		ReturnType->Static = false;
		ReturnType->DataType = NumericTypeT::DataTypeT::Int;
		FunctionType = llvm::FunctionType::get(
			ReturnType->GenerateLLVMType(Context),
			std::vector<llvm::Type *>
			{
				llvm::IntegerType::get(LLVM, 32), 
				llvm::PointerType::get(llvm::PointerType::get(llvm::IntegerType::get(LLVM, 8), 0), 0)
			}, 
			false);
	}
	else FunctionType = llvm::FunctionType::get(llvm::Type::getVoidTy(LLVM), false);
	auto Function = llvm::Function::Create(FunctionType, llvm::Function::ExternalLinkage, Entry ? "main" : "__ctor", Module);
	auto Block = llvm::BasicBlock::Create(LLVM, "entrypoint", Function);
	
	Assert(!Context.Module);
	Context.Module = Module;
	Assert(!Context.Block);
	Context.Block = Block;
	Assert(!Context.Scope);
	Assert(Context.IsConstant);
	
	DynamicT *ReturnValue = nullptr;
	
	if (Entry)
	{
		ReturnValue = new DynamicT(Context.Position);
		ReturnValue->Type = ReturnType;
		ReturnValue->Target = new llvm::AllocaInst(ReturnType->GenerateLLVMType(Context), "", Context.Block);
		ReturnValue->Initialized = false;
		auto Out = TopGroup->AccessElement(Context, FunctionOutputKey).As<AssignableT>();
		Out->Assign(Context, ReturnValue);
	}

	Top->Simplify(Context); // TODO position
	
	if (Entry)
	{
		llvm::ReturnInst::Create(Context.LLVM, ReturnValue->GenerateLLVMLoad(Context), Block);
	}
	else
	{
		auto GlobalConstructorStructType = llvm::StructType::get(
			LLVM,
			std::vector<llvm::Type *>{
				llvm::IntegerType::get(LLVM, 32),
				llvm::PointerType::get(FunctionType, 0)}, 
			false);

		auto GlobalConstructorArrayType = llvm::ArrayType::get(GlobalConstructorStructType, 1);

		new llvm::GlobalVariable
		(
			*Module, 
			GlobalConstructorArrayType, 
			false, 
			llvm::GlobalValue::AppendingLinkage, 
			llvm::ConstantArray::get
			(
				GlobalConstructorArrayType, 
				std::vector<llvm::Constant *>
				{
					llvm::ConstantStruct::get
					(
						GlobalConstructorStructType, 
						std::vector<llvm::Constant *>{
							llvm::ConstantInt::get(llvm::IntegerType::get(Context.LLVM, 32), 65535, false),
							Function
						}
					)
				}
			), 
			"llvm.global_ctors"
		);
		
		llvm::ReturnInst::Create(Context.LLVM, Block);
	}

	Module->dump();
}

//================================================================================================================
// Variants
ClassifyT::ClassifyT(PositionT const Position) : NucleusT(Position) {}

AtomT ClassifyT::Clone(void)
{
	auto Out = new ClassifyT;
	Out->Value = Value->Clone();
	Out->Class = Class->Clone();
	Out->Body = Body->Clone();
	Out->Else = Else->Clone();
	return Out;
}

/* Structure
const variant
	type -> varianttype
	* valuetype -> variantclass
	* value -> ???
		type -> actual type
dynamic
	type -> varianttype
	target -> memory
variantclass
	basetype -> some type
*/
void ClassifyT::Simplify(ContextT Context)
{
	Subject->Simplify(Context);
	Classification->Simplify(Context);
	
	auto ClassificationGroup = Classification.As<GroupT>();
	if (!ClassificationGroup) ERROR;
	std::string DestName;
	AtomT DestVariantClassAtom;
	for (auto &Pair : **ClassificationGroup)
	{
		if (Type) ERROR; // Max 1 in class group
		DestName = Pair.first;
		DestVariantClassAtom = Pair.second;
	}
	if (!DestVariantClassAtom) ERROR; // Min 1 in class group
	auto DestVariantClass = DestVariantClassAtom.As<VariantClassT>();
	
	if (auto Variant = Subject.As<VariantT>())
	{
		if (Variant->ValueClass == DestVariantClass) // TODO Is this cool?
		{
			{
				auto BodyGroup = Body.As<GroupT>();
				if (!BodyGroup) ERROR;
				auto Dest = BodyGroup->AccessElement(Context, Name);
				auto DestAssignable = Dest.As<AssignableT>();
				DestAssignable->Assign(Context, Variant->Value);
			}
			Body->Simplify(Context);
		}
		else if (Else)
		{
			Assert(Else.As<GroupT>());
			Else->Simplify(Context);
		}
	}
	else
	{
		auto IfBlock = llvm::BasicBlock::Create(Context.LLVM, "", Context.Block->GetParent());
		auto ElseBlock = llvm::BasicBlock::Create(Context.LLVM, "", Context.Block->GetParent());
		auto EndBlock = llvm::BasicBlock::Create(Context.LLVM, "", Context.Block->GetParent());
		
		{
			auto Dynamic = Subject.As<DynamicT>();
			Assert(Dynamic);
			if (Dynamic->Initialized) ERROR;
			llvm::BranchInst::Create(
				IfBlock, 
				ElseBlock, 
				new llvm::ICmpInst(
					Context.Block, 
					llvm::CmpInst::ICMP_EQ, 
					GetElementPointer(Dynamic->Target, 0, Context.Block),
					DestVariantClass->GenerateLLVMLoadID(Context)),
				Context.Block);
		}
		
		{
			auto IfContext = Context;
			IfContext.Block = IfBlock;
			
			auto Dynamic = new DynamicT(Context.Position);
			Dynamic->Type = DestVariantClass->BaseType;
			auto DestType = Dynamic->Type.As<LLVMLoadableTypeT>();
			Dynamic->Target = new llvm::BitCastInst(
				GetElementPointer(Dynamic->Target, 1, Context.Block), 
				DestType->GenerateLLVMType(Context),
				"",
				IfContext.Block);
			Dynamic->Initialized = true;
			
			auto Group = Body.As<GroupT>();
			if (!Group) ERROR;
			auto Resolution = BodyGroup->AccessElement(Context, Name);
			auto ResolutionAssignable = Resolution.As<AssignableT>();
			ResolutionAssignable->Assign(Context, Dynamic);
			
			// TODO branch initialized
			Body->Simplify(IfContext);
			// TODO unbranch initialized
			
			llvm::BranchInst::Create(EndBlock, IfContext.Block);
		}
		
		if (Else)
		{
			auto ElseContext = Context;
			ElseContest.Block = ElseBlock;
			
			// TODO branch initialized
			Else->Simplify(ElseContext);
			// TODO unbranch initialized
			
			llvm::BranchInst::Create(EndBlock, ElseContext.Block);
		}
		
		// TODO modify context block
	}
}

}

