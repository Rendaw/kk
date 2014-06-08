#include "core.h"

namespace Core
{

constexpr TypeIDT DefaultTypeID = 0;

PositionBaseT::~PositionBaseT(void) {}

HardPositionT::HardPositionT(char const *File, char const *Function, int Line) : Position(::StringT() << "(HARD)" << File << "/" << Function << ":" << Line) {}

std::string HardPositionT::AsString(void) const { return Position; }


llvm::Value *GetElementPointer(ContextT &Context, llvm::Value *Base, int32_t Index, llvm::BasicBlock *Block)
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

AtomT NucleusT::GetType(ContextT &Context) { ERROR; }

void NucleusT::Simplify(ContextT &Context) {}

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
AtomT &AtomT::operator =(AtomT const &Other) { Set(Other.Nucleus); return *this; }

AtomT::operator NucleusT *(void) { return Nucleus; }
AtomT::operator bool(void) const { return Nucleus; }
bool AtomT::operator !(void) const { return !Nucleus; }

NucleusT *AtomT::operator ->(void) { return Nucleus; }

ContextT::ContextT(llvm::LLVMContext &LLVM) : LLVM(LLVM), Module(nullptr), EntryBlock(nullptr), IsConstant(true) 
{ 
	make_shared(Block);
	make_shared(TypeIDCounter, static_cast<unsigned short>(0));
}
	
BranchableBoolT::ValueT::ValueT(PositionT const Position, bool Value) : Position(Position), Value(Value) {}

BranchableBoolT::ValueT::ValueT(ValueT const &Other) : Position(Other.Position), Value(Other.Value) {}

BranchableBoolT::BranchableBoolT(PositionT const Position, OptionalT<ValueT> Base) : Position(Position), Base(Base) {}

BranchableBoolT &BranchableBoolT::operator =(BranchableBoolT const &Other)
{
	Position = Other.Position;
	Base = Other.Base;
	Model = Other.Model;
	if (Other.Branch)
	{
		Branch.Set(std::remove_reference<decltype(*Branch)>::type{});
		make_shared(*Branch);
		**Branch = **Other.Branch;
	}
	return *this;
}
	
bool BranchableBoolT::Get(void)
{
	if (Branch) return (*Branch)->Get();
	else 
	{
		if (Base) return Base->Value;
		else return false;
	}
}

void BranchableBoolT::Set(ContextT &Context)
{
	if (Branch) (*Branch)->Set(Context);
	else 
	{
		AssertOr(!Base, !Base->Value);
		Base = ValueT{Context.Position, true};
	}
}

void BranchableBoolT::Unset(ContextT &Context)
{
	if (Branch) (*Branch)->Unset(Context);
	else 
	{
		Assert(Base);
		Assert(Base->Value);
		Base = ValueT{Context.Position, false};
	}
}

void BranchableBoolT::EnterBranch(ContextT &Context) 
{
	if (Branch) (*Branch)->EnterBranch(Context);
	else
	{
		make_shared(*Branch, Context.Position, Base);
	}
}

void BranchableBoolT::ExitBranch(ContextT &Context)
{
	if (Branch && (*Branch)->Branch) (*Branch)->ExitBranch(Context);
	else
	{
		Assert(Branch);
		if (Model)
		{
			if ((*Branch)->Base)
			{
				if ((*Branch)->Base->Value != Model->Value) ERROR;
			}
			else if (Model->Value) ERROR;
		}
		else
		{
			if ((*Branch)->Base)
				Model = (*Branch)->Base;
			else Model = ValueT((*Branch)->Position, false);
		}
		Branch.Unset();
	}
}

void BranchableBoolT::Finish(void)
{
	if (Branch) (*Branch)->Finish();
	else 
	{
		if (Model) Base = Model;
	}
}

//================================================================================================================
// Interfaces
ValueT::~ValueT(void) {}

LLVMLoadableT::~LLVMLoadableT(void) {}

LLVMLoadableTypeT::~LLVMLoadableTypeT(void) {}

//================================================================================================================
// Basics
UndefinedT::UndefinedT(PositionT const Position) : NucleusT(Position) {}

void UndefinedT::Assign(ContextT &Context, AtomT Other)
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

AtomT ImplementT::GetType(ContextT &Context) { return Type; }

void ImplementT::Simplify(ContextT &Context)
{
	if (Value) 
	{
		Value->Simplify(Context);
		if (!Type) Type = Value->GetType(Context);
	}
	if (!Type) { ERROR; }
	else Type->Simplify(Context);
	auto Allocable = Type.As<TypeT>();
	if (!Allocable) ERROR;
	Replace(Allocable->Allocate(Context, Value ? Value : AtomT()));
}

//================================================================================================================
// Primitives
LLVMAssignableTypeT::~LLVMAssignableTypeT(void) {}

TypeT::~TypeT(void) {}

StringT::StringT(PositionT const Position) : NucleusT(Position) {}

AtomT StringT::Clone(void)
{
	auto Out = new StringT(Position);
	Out->Initialized = Initialized;
	Out->Type = Type;
	Out->Data = Data;
	return Out;
}

AtomT StringT::GetType(ContextT &Context)
{
	if (!Type) Type = new StringT(Position);
	return Type;
}

void StringT::Assign(ContextT &Context, AtomT Other)
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

bool StringTypeT::IsVariable(void)
{
	return false;
}

void StringTypeT::CheckType(ContextT &Context, AtomT Other)
{
	auto OtherType = Other->GetType(Context).As<StringTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
}
	
AtomT StringTypeT::Allocate(ContextT &Context, AtomT Value)
{
	auto Out = new StringT(Context.Position);
	Out->Type = this;
	if (Value)
	{
		if (auto String = Value.As<StringT>())
		{
			if (!String->Initialized.Get()) ERROR;
			Out->Data = String->Data;
		}
		else ERROR;
		Out->Initialized.Set(Context);
	}
	return Out;
}

void StringTypeT::Assign(ContextT &Context, BranchableBoolT &Initialized, std::string &Data, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized.Get() && Static) ERROR;
	auto String = Other.As<StringT>();
	if (!String) ERROR;
	if (!String->Initialized.Get()) ERROR;
	Data = String->Data;
	Initialized.Set(Context);
}

template <typename DataT> NumericT<DataT>::NumericT(PositionT const Position) : NucleusT(Position) {}

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

template <typename DataT> AtomT NumericT<DataT>::GetType(ContextT &Context)
{
	if (!Type) 
	{
		auto Temp = new NumericTypeT(Position);
		Temp->DataType = GetDataType(ExplicitT<DataT>());
		Type = Temp;
	}
	return Type;
}

template <typename DataT> void NumericT<DataT>::Assign(ContextT &Context, AtomT Other)
{
	auto Type = GetType(Context).template As<NumericTypeT>();
	Type->CheckType(Context, Other);
	if (Type->Static && Initialized.Get()) ERROR;
	auto Number = Other.As<NumericT<DataT>>();
	if (!Number) ERROR;
	if (!Number->Initialized.Get()) ERROR;
	Data = Number->Data;
}

template <> llvm::Value *NumericT<float>::GenerateLLVMLoad(ContextT &Context)
{
	if (!Initialized.Get()) ERROR;
	return llvm::ConstantFP::get
		(
			llvm::Type::getFloatTy(Context.LLVM),
			Data
		);
}

template <> llvm::Value *NumericT<double>::GenerateLLVMLoad(ContextT &Context)
{
	if (!Initialized.Get()) ERROR;
	return llvm::ConstantFP::get
		(
			llvm::Type::getDoubleTy(Context.LLVM),
			Data
		);
}

template <typename DataT> llvm::Value *NumericT<DataT>::GenerateLLVMLoad(ContextT &Context)
{
	if (!Initialized.Get()) ERROR;
	return llvm::ConstantInt::get
		(
			llvm::IntegerType::get(Context.LLVM, sizeof(Data) * 8), 
			Data, 
			false
		);
}

VariableT::VariableT(PositionT const Position) : NucleusT(Position), Target(nullptr) {}

AtomT VariableT::GetType(ContextT &Context) { return Type; }

llvm::Value *VariableT::GetTarget(ContextT &Context)
{
	if (!Target)
	{
		Assert(StructTarget);
		Target = GetElementPointer(Context, StructTarget->Struct, StructTarget->Index, *Context.Block);
	}
	return Target;
}

void VariableT::Assign(ContextT &Context, AtomT Other)
{
	auto Type = GetType(Context).As<LLVMAssignableTypeT>();
	if (!Type) ERROR;
	Type->AssignLLVM(Context, Initialized, GetTarget(Context), Other);
}

llvm::Value *VariableT::GenerateLLVMLoad(ContextT &Context)
{
	if (!Initialized.Get()) ERROR;
	return new llvm::LoadInst(GetTarget(Context), "", *Context.Block); 
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

bool NumericTypeT::IsVariable(void) 
{
	return !Constant;
}

bool NumericTypeT::IsSigned(void) const
{
	return DataType == DataTypeT::Int;
}

void NumericTypeT::CheckType(ContextT &Context, AtomT Other)
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

template <typename BaseT> AtomT NumericTypeConstantAssign(ContextT &Context, NumericTypeT &Type, AtomT Value)
{
	auto Out = new NumericT<BaseT>(Context.Position);
	Out->Type = &Type;
	if (Value)
	{
		if (auto Number = Value.As<NumericT<int32_t>>()) { if (!Number->Initialized.Get()) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<uint32_t>>()) { if (!Number->Initialized.Get()) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<float>>()) { if (!Number->Initialized.Get()) ERROR; Out->Data = Number->Data; }
		else if (auto Number = Value.As<NumericT<double>>()) { if (!Number->Initialized.Get()) ERROR; Out->Data = Number->Data; }
		else ERROR;
		Out->Initialized.Set(Context);
	}
	return Out;
}

AtomT NumericTypeT::Allocate(ContextT &Context, AtomT Value)
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
		
		auto Out = new VariableT(Context.Position);
		Out->Type = this;
		
		auto Dest = new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.EntryBlock);
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
			*Context.Block,
			Source,
			SourceType,
			SourceSigned,
			DestType,
			DestSigned);
		new llvm::StoreInst(Source, Dest, *Context.Block);
		
		Out->Initialized.Set(Context);
		return Out;
	}
}

void NumericTypeT::AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized.Get() && Static) ERROR;
	Initialized.Set(Context);
	auto Loadable = Other.As<LLVMLoadableT>();
	if (!Loadable) ERROR;
	new llvm::StoreInst(Loadable->GenerateLLVMLoad(Context), Target, *Context.Block);
}

llvm::Type *NumericTypeT::GenerateLLVMType(ContextT &Context)
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

GroupT::GroupT(PositionT const Position) : NucleusT(Position), IsFunctionTop(false) {}

AtomT GroupT::Clone(void)
{
	auto Out = new GroupT(Position);
	for (auto &Statement : Statements)
		Out->Statements.push_back(Statement->Clone());
	for (auto &Pair : *this)
		Out->Add(Pair.first, Pair.second->Clone());
	return Out;
}

void GroupT::Simplify(ContextT &Context)
{
	for (auto &Statement : Statements)
	{
		auto Subcontext = Context;
		Subcontext.Scope = this;
		Statement->Simplify(Subcontext);
	}
}

void GroupT::Assign(ContextT &Context, AtomT Other)
{
	auto Group = Other.As<GroupT>();
	if (!Group) ERROR;
	for (auto &Pair : *this)
	{
		auto Dest = Pair.second.As<ValueT>();
		if (!Dest) ERROR;
		auto Source = Group->GetByKey(Pair.first);
		if (!Source) ERROR;
		auto Subcontext = Context;
		Subcontext.Scope = this;
		Dest->Assign(Subcontext, *Source);
	}
}

AtomT GroupT::AccessElement(ContextT &Context, std::string const &Key)
{
	auto Got = GetByKey(Key);
	if (Got) return *Got;
	auto Out = new UndefinedT(Context.Position);
	Add(Key, Out);
	return Out;
}

AtomT GroupT::AccessElement(ContextT &Context, AtomT Key)
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

void BlockT::Simplify(ContextT &Context) {}

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

void ElementT::Simplify(ContextT &Context)
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
AsVariableTypeT::AsVariableTypeT(PositionT const Position) : NucleusT(Position) {}

AtomT AsVariableTypeT::Clone(void)
{
	auto Out = new AsVariableTypeT(Position);
	if (Type)
		Out->Type = Type->Clone();
	return Out;
}

void AsVariableTypeT::Simplify(ContextT &Context) 
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

void AssignmentT::Simplify(ContextT &Context)
{
	Context.Position = Position;
	Left->Simplify(Context);
	Right->Simplify(Context);
	auto Assignable = Left.As<ValueT>();
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

void FunctionTypeT::Simplify(ContextT &Context)
{
	Signature->Simplify(Context);
}

AtomT FunctionTypeT::Allocate(ContextT &Context, AtomT Value) 
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
		auto Variable = new VariableT(Context.Position);
		Variable->Type = this;
		Variable->Target = new llvm::AllocaInst(llvm::PointerType::getUnqual(GenerateLLVMType(Context)), "", Context.EntryBlock);
		if (Value) Variable->Assign(Context, Function);
		return Variable;
	}
}

bool FunctionTypeT::IsVariable(void)
{
	return !Constant;
}

void FunctionTypeT::CheckType(ContextT &Context, AtomT Other)
{
	AssertNE(ID, 0);
	auto OtherType = Other->GetType(Context).As<FunctionTypeT>();
	if (!OtherType) ERROR;
	if (OtherType->ID != ID) ERROR;
}

llvm::Type *FunctionTypeT::GenerateLLVMType(ContextT &Context)
{
	auto Result = ProcessFunction(Context, GenerateLLVMTypeParamsT{});
	return Result.Get<GenerateLLVMTypeResultsT>().Type;
}
	
void FunctionTypeT::AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized.Get() && Static) ERROR;
	if (auto Function = Other.As<FunctionT>())
	{
		auto Result = ProcessFunction(Context, GenerateLLVMLoadParamsT{*Function});
		new llvm::StoreInst(Result.Get<GenerateLLVMLoadResultsT>().Value, Target, *Context.Block);
	}
	else if (auto Variable = Other.As<VariableT>())
	{
		new llvm::StoreInst(Variable->GenerateLLVMLoad(Context), Target, *Context.Block);
	}
	else ERROR;
	Initialized.Set(Context);
}

AtomT FunctionTypeT::Call(ContextT &Context, AtomT Function, AtomT Input)
{
	auto Result = ProcessFunction(Context, CallParamsT{Function, Input});
	return Result.Get<CallResultsT>().Result;
}

std::vector<uint8_t> GetLLVMArgID(ExplicitT<VariableT>)
{
	return {0};
}

std::vector<uint8_t> GetLLVMArgID(ExplicitT<GroupT>)
{
	return {1};
}

std::vector<uint8_t> GetLLVMArgID(AtomT const &Value)
{
	if (auto Variable = Value.As<VariableT>())
	{
		return GetLLVMArgID(ExplicitT<VariableT>());
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
FunctionTypeT::ProcessFunctionResultT FunctionTypeT::ProcessFunction(ContextT &Context, ProcessFunctionParamT Param)
{
	FunctionTreeT<FunctionT::CachedLLVMFunctionT> *FunctionTree = nullptr;
	AtomT CallInput;
	
	AtomT ParentScope;
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
		ParentScope = Params.Function->ParentScope;
		FunctionTree = &Params.Function->InstanceTree;
	}
	else if (Param.Is<CallParamsT>())
	{
		auto &Params = Param.Get<CallParamsT>();
		if (auto Function = Params.Function.As<FunctionT>())
		{
			Body = Function->Body;
			ParentScope = Function->ParentScope;
			FunctionTree = &Function->InstanceTree;
		}
		else if (auto Variable = Params.Function.As<VariableT>())
		{
			LLVMFunction = Variable->GenerateLLVMLoad(Context);
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
		BodyGroup->Parent = ParentScope;
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
	std::vector<llvm::Value *> VariableCallInput; // Values passed to llvm::callinst
	std::vector<VariableT *> VariableCallOutput; // Variables parsed from result of llvm::callinst
	
	// INSIDE
	AtomT BodyInput;
	AtomT BodyOutput;
	if (Body) 
	{
		BodyInput = (*BodyGroup)->AccessElement(Context, FunctionInputKey);
		BodyOutput = (*BodyGroup)->AccessElement(Context, FunctionOutputKey);
	}
	std::vector<VariableT *> VariableBodyOutput;
	std::vector<VariableT *> VariableBodyInput;
	
	if (OutputType)
	{
		std::function<void(AtomT Type, AtomT Body, AtomT Call)> ClassifyOutput;
		ClassifyOutput = [&](AtomT Type, AtomT Body, AtomT Call)
		{
			OptionalT<ValueT *> BodyAssignable;
			if (Body)
			{
				BodyAssignable = Body.As<ValueT>();
				Assert(*BodyAssignable);
			}
			
			OptionalT<ValueT *> CallAssignable;
			if (Param.Is<CallParamsT>())
			{
				CallAssignable = Call.As<ValueT>();
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
				if (SimpleType->IsVariable())
				{
					FunctionContext.IsConstant = false;
					
					auto LLVMType = Type.As<LLVMLoadableTypeT>();
					Assert(LLVMType);
					LLVMReturnTypes.push_back(LLVMType->GenerateLLVMType(Context));
					
					if (Body)
					{
						auto BodyVariable = new VariableT(Context.Position);
						BodyVariable->Type = Type;
						VariableBodyOutput.push_back(BodyVariable);
						BodyAssignable->Assign(Context, BodyVariable);
					}
					
					if (Param.Is<CallParamsT>())
					{
						auto CallVariable = new VariableT(Context.Position);
						CallVariable->Type = Type;
						VariableCallOutput.push_back(CallVariable);
						CallAssignable->Assign(Context, CallVariable);
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
			Assert(VariableBodyOutput.size() == 1);
			VariableBodyOutput[0]->Target = new llvm::AllocaInst(LLVMReturnTypes[0], "", Context.EntryBlock);
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
				auto LLVMResultStruct = new llvm::AllocaInst(LLVMResultStructType, "", Context.EntryBlock);
				VariableCallInput.push_back(LLVMResultStruct);
				
				size_t StructIndex = 0;
				for (auto &Variable : VariableCallOutput)
					Variable->StructTarget = VariableT::StructTargetT{LLVMResultStruct, StructIndex++};
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
			
			OptionalT<ValueT *> BodyAssignable;
			if (Body)
			{
				auto OptBodyAssignable = Body.As<ValueT>();
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
				
				if (SimpleType->IsVariable())
				{
					FunctionContext.IsConstant = false;
					
					auto LLVMType = Type.As<LLVMLoadableTypeT>();
					Assert(LLVMType);
					LLVMArgTypes.push_back(LLVMType->GenerateLLVMType(Context));
					
					if (Param.Is<CallParamsT>())
					{
						auto CallValue = Call.As<LLVMLoadableT>();
						if (!CallValue) ERROR;
						VariableCallInput.push_back(CallValue->GenerateLLVMLoad(Context));
					}
					else
					{
						TypeLookup.Enter(GetLLVMArgID(ExplicitT<VariableT>()));
						if (FunctionLookup)
							FunctionLookup->Enter(GetLLVMArgID(ExplicitT<VariableT>()));
					}
					
					if (Body)
					{
						auto BodyVariable = new VariableT(Context.Position);
						VariableBodyInput.push_back(BodyVariable);
						BodyAssignable->Assign(Context, BodyVariable);
					}
				}
				else
				{
					if (Param.Is<GenerateLLVMTypeParamsT>() || Param.Is<GenerateLLVMLoadParamsT>())
						ERROR;
					Assert(Body);
					Assert(Param.Is<CallParamsT>());
					auto BodyAtom = SimpleType->Allocate(Context, {});
					auto BodyValue = BodyAtom.As<ValueT>();
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
					AssertE(VariableBodyOutput.size(), LLVMReturnTypes.size());
					for (auto &Variable : VariableBodyOutput)
						Variable->StructTarget = VariableT::StructTargetT{&Argument, StructIndex++};
				}
				else
				{
					AssertLT(InputIndex, VariableBodyInput.size());
					VariableBodyInput[InputIndex++]->Target = &Argument;
				}
				++Index;
			}
			AssertE(InputIndex, VariableBodyInput.size());
		}
		
		FunctionContext.EntryBlock = Block;
		*FunctionContext.Block = Block;
		(*BodyGroup)->Simplify(FunctionContext);
		
		if (LLVMReturnTypes.size() == 1)
			llvm::ReturnInst::Create(Context.LLVM, VariableBodyOutput[0]->GenerateLLVMLoad(Context), Block);
		else llvm::ReturnInst::Create(Context.LLVM, Block);
	}

	if (Param.Is<GenerateLLVMLoadParamsT>()) return GenerateLLVMLoadResultsT{LLVMFunction}; // NOTE Return
	
	if (!FunctionContext.IsConstant)
	{
		Context.IsConstant = false;
		auto Result = llvm::CallInst::Create(LLVMFunction, VariableCallInput, "", *Context.Block);
		
		if (LLVMReturnTypes.size() == 1)
		{
			AssertE(VariableCallOutput.size(), 1u);
			VariableCallOutput[0]->Target = Result;
		}
	}
	
	return CallResultsT{BodyOutput}; // NOTE Return
}

FunctionT::FunctionT(PositionT const Position) : NucleusT(Position) {}

AtomT FunctionT::GetType(ContextT &Context) { return Type; }

void FunctionT::Simplify(ContextT &Context)
{
	Type->Simplify(Context);
	ParentScope = Context.Scope;
}

AtomT FunctionT::Call(ContextT &Context, AtomT Input)
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

void CallT::Simplify(ContextT &Context)
{
	Function->Simplify(Context);
	Input->Simplify(Context);
	AtomT Type;
	if (auto Function = this->Function.As<FunctionT>())
		Type = Function->GetType(Context);
	else if (auto Variable = this->Function.As<VariableT>())
	{
		if (auto FunctionType = Variable->Type.As<FunctionTypeT>())
			Type = *FunctionType;
		else ERROR;
	}
	else ERROR;
	auto FunctionType = Type.As<FunctionTypeT>();
	if (!FunctionType) ERROR;
	Replace(FunctionType->Call(Context, Function, Input));
}

ModuleT::ModuleT(PositionT const Position) : NucleusT(Position), Entry(false) {}

void ModuleT::Simplify(ContextT &Context)
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
	Assert(!*Context.Block);
	*Context.Block = Block;
	Assert(!Context.Scope);
	Assert(Context.IsConstant);
	
	VariableT *ReturnValue = nullptr;
	
	if (Entry)
	{
		ReturnValue = new VariableT(Context.Position);
		ReturnValue->Type = ReturnType;
		ReturnValue->Target = new llvm::AllocaInst(ReturnType->GenerateLLVMType(Context), "", Context.EntryBlock);
		auto Out = TopGroup->AccessElement(Context, FunctionOutputKey).As<ValueT>();
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
VariantValueT::VariantValueT(PositionT const Position) : NucleusT(Position) {}

void VariantValueT::Assign(ContextT &Context, AtomT Other)
{
	GetType(Context).As<VariantTypeT>()->Assign(Context, Initialized, Data, Other);
}

VariantClassT::VariantClassT(PositionT const Position) : NucleusT(Position), Index(0), Constant(true), Static(true) {}

AtomT VariantClassT::Allocate(ContextT &Context, AtomT Value)
{
	auto VariantType = Variant.As<VariantTypeT>();
	if (!VariantType) ERROR;
	return VariantType->Allocate(Context, Value, Constant, Static, this);
}

bool VariantClassT::IsVariable(void) { Assert(false); return false; }

void VariantClassT::CheckType(ContextT &Context, AtomT Other) 
{ 
	auto Type = BaseType.As<TypeT>();
	if (!Type) ERROR;
	Type->CheckType(Context, Other);
}

llvm::Value *VariantClassT::GenerateLLVMLoadID(ContextT &Context)
{
	auto VariantType = Variant.As<VariantTypeT>();
	return llvm::ConstantInt::get
	(
		VariantType->GenerateLLVMIndexType(Context),
		Index, 
		false
	);
}

VariantTypeT::VariantTypeT(PositionT const Position) : GroupT(Position), ID(DefaultTypeID), Constant(true), Static(true) {}

AtomT VariantTypeT::Allocate(ContextT &Context, AtomT Value)
{
	return Allocate(Context, Value, Constant, Static, {});
}

AtomT VariantTypeT::Allocate(ContextT &Context, AtomT Value, bool Constant, bool Static, AtomT Class)
{
	AtomT ValueType;
	if (Value)
	{
		if (Class)
		{
			auto SpecificClass = Class.As<VariantClassT>();
			if (!SpecificClass) ERROR;
			SpecificClass->CheckType(Context, Value);
			ValueType = SpecificClass->BaseType;
		}
		else
		{
			auto VariantValue = Value.As<VariantValueT>();
			if (!VariantValue) ERROR;
			Class = VariantValue->Class;
			ValueType = VariantValue->Data->GetType(Context);
		}
	}
	
	if (Constant)
	{
		auto Out = new VariantValueT(Context.Position);
		Out->Variant = this;
		if (Value)
		{
			Assert(Class);
			Out->Class = Class;
			Out->Data = ValueType.As<TypeT>()->Allocate(Context, Value);
		}
		return Out;
	}
	else
	{
		auto Out = new VariableT(Context.Position);
		Out->Type = this;
		Out->Target = new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.EntryBlock);
		if (Value) AssignLLVM(Context, Out->Initialized, Out->Target, Value);
		return Out;
	}
}

bool VariantTypeT::IsVariable(void) { return !Constant; }

void VariantTypeT::CheckType(ContextT &Context, AtomT Other)
{
	VariantTypeT *OtherType = nullptr;
	if (auto Value = Other.As<VariantValueT>())
	{
		OtherType = *Value->Variant.As<VariantTypeT>();
	}
	else if (auto Class = Other.As<VariantClassT>())
	{
		OtherType = *Class->Variant.As<VariantTypeT>();
	}
	else ERROR;
	if (OtherType->ID != ID) ERROR;
}
	
llvm::Type *VariantTypeT::GenerateLLVMType(ContextT &Context)
{
	if (!LLVMType)
	{
		GenerateLLVMIndexType(Context);
		
		llvm::DataLayout DataLayout(Context.Module);
		llvm::Type *HighestAlignmentType = nullptr;
		size_t Largest = 0;
		size_t HighestAlignment = 0;
		for (auto &Pair : *this)
		{
			auto SpecificClass = Pair.second.As<VariantClassT>();
			auto Type = SpecificClass->BaseType.As<LLVMLoadableTypeT>();
			if (!Type) ERROR;
			auto LLVMType = Type->GenerateLLVMType(Context);
			auto Size = DataLayout.getTypeStoreSize(LLVMType);
			auto Alignment = DataLayout.getPrefTypeAlignment(LLVMType);
			if (Size > Largest)
				Largest = Size;
			if (Alignment > HighestAlignment)
			{
				HighestAlignment = Alignment;
				HighestAlignmentType = LLVMType;
			}
		}
		
		if (!HighestAlignmentType) ERROR; // Or maybe handle this edge case?
		
		std::vector<llvm::Type *> StructTypes;
		if (HighestAlignmentType)
		{
			StructTypes.push_back(HighestAlignmentType);
			if (Largest > HighestAlignment)
				StructTypes.push_back(llvm::ArrayType::get(llvm::IntegerType::get(Context.LLVM, 8), Largest - HighestAlignment));
		}
		LLVMType = llvm::StructType::create(
			Context.LLVM, 
			std::vector<llvm::Type *>{
				LLVMIndexType, 
				llvm::StructType::create(Context.LLVM, StructTypes),
			},
			"", 
			false);
	}
	return LLVMType;
}

void VariantTypeT::Assign(ContextT &Context, BranchableBoolT Initialized, AtomT &Data, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized.Get() && Static) ERROR;
	auto Value = Other.As<VariantValueT>();
	if (!Value) ERROR;
	if (!Value->Initialized.Get()) ERROR;
	Data = Value->Data;
	Initialized.Set(Context);
}

void VariantTypeT::AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other)
{
	CheckType(Context, Other);
	if (Initialized.Get() && Static) ERROR;
	if (auto Variant = Other.As<VariantValueT>())
	{
		auto SpecificClass = Variant->Class.As<VariantClassT>();
		auto LoadableType = SpecificClass->BaseType.As<LLVMLoadableTypeT>();
		auto AssignableType = SpecificClass->BaseType.As<LLVMAssignableTypeT>();
		BranchableBoolT TempInitialized;
		AssignableType->AssignLLVM(
			Context, 
			TempInitialized, 
			new llvm::BitCastInst(
				GetElementPointer(Context, Target, 1, *Context.Block), 
				LoadableType->GenerateLLVMType(Context),
				"",
				*Context.Block),
			Variant->Data);
		new llvm::StoreInst(
			SpecificClass->GenerateLLVMLoadID(Context), 
			GetElementPointer(Context, Target, 0, *Context.Block),
			*Context.Block);
	}
	else if (auto Variable = Other.As<VariableT>())
	{
		llvm::DataLayout DataLayout(Context.Module);
		auto Copy = 
			DataLayout.getPointerSizeInBits() == 32 ?
				Context.Module->getFunction("llvm.memcpy.p0i8.p0i8.i32") :
				Context.Module->getFunction("llvm.memcpy.p0i8.p0i8.i64");
		Assert(Copy);
		auto CopyType = llvm::PointerType::getUnqual(llvm::IntegerType::get(Context.LLVM, 8));
		llvm::CallInst::Create(
			Copy, 
			std::vector<llvm::Value *>{
				new llvm::BitCastInst(Target, CopyType, "", *Context.Block),
				new llvm::BitCastInst(Variable->Target, CopyType, "", *Context.Block),
				llvm::ConstantExpr::getSizeOf(GenerateLLVMType(Context)),
				llvm::ConstantExpr::getAlignOf(GenerateLLVMType(Context)),
				llvm::ConstantInt::get
				(
					llvm::IntegerType::get(Context.LLVM, 1), 
					0, 
					false
				)
			}, 
			"", 
			*Context.Block);
	}
	else ERROR;
	Initialized.Set(Context);
}

llvm::Type *VariantTypeT::GenerateLLVMIndexType(ContextT &Context)
{
	if (!LLVMIndexType)
	{
		size_t IntSize = 1;
		size_t ClassCount = 0;
		for (auto &Class : *this) 
		{
			(void)Class;
			++ClassCount;
			if ((2u << (IntSize - 1u)) < ClassCount) ++IntSize;
		}
		LLVMIndexType = llvm::IntegerType::get(Context.LLVM, IntSize);
	}
	return LLVMIndexType;
}

VariantT::VariantT(PositionT const Position) : NucleusT(Position) {}

AtomT VariantT::Clone(void)
{
	auto Out = new VariantT(Position);
	Out->Types = Types->Clone();
	return Out;
}

void VariantT::Simplify(ContextT &Context)
{
	if (!Types) ERROR;
	Types->Simplify(Context);
	auto TypesGroup = Types.As<GroupT>();
	if (!TypesGroup) ERROR;
	
	auto VariantType = new VariantTypeT(Position);
	VariantType->ID = (*Context.TypeIDCounter)++;
	size_t ClassIndex = 0;
	for (auto &Pair : **TypesGroup)
	{
		auto Class = new VariantClassT(Position);
		Class->Index = ClassIndex++;
		Class->Variant = VariantType;
		Class->BaseType = Pair.second;
		auto ClassElement = VariantType->AccessElement(Context, Pair.first);
		ClassElement.As<ValueT>()->Assign(Context, Class);
	}
	Replace(VariantType);
}

IdentifyT::IdentifyT(PositionT const Position) : NucleusT(Position) {}

AtomT IdentifyT::Clone(void)
{
	auto Out = new IdentifyT(Position);
	Out->Subject = Subject->Clone();
	Out->Criteria = Criteria->Clone();
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
void BranchValues(ContextT &Context, AtomT &GroupAtom, bool GoingUp = true)
{
	auto Group = GroupAtom.As<GroupT>();
	Assert(Group);
	for (auto &Pair : **Group)
	{
		if (auto Value = Pair.second.As<ValueT>())
			Value->Initialized.EnterBranch(Context);
		else if (auto Group = Pair.second.As<GroupT>())
			BranchValues(Context, Pair.second, false);
		else Assert(false); // Shouldn't happen, I think
	}
	if (GoingUp && !Group->IsFunctionTop && Group->Parent)
		BranchValues(Context, *Group->Parent);
}

void UnbranchValues(ContextT &Context, AtomT &GroupAtom, bool GoingUp = true)
{
	auto Group = GroupAtom.As<GroupT>();
	Assert(Group);
	for (auto &Pair : **Group)
	{
		if (auto Value = Pair.second.As<ValueT>())
			Value->Initialized.ExitBranch(Context);
		else if (auto Group = Pair.second.As<GroupT>())
			UnbranchValues(Context, Pair.second, false);
		else Assert(false); // Shouldn't happen, I think
	}
	if (GoingUp && !Group->IsFunctionTop && Group->Parent)
		UnbranchValues(Context, *Group->Parent);
}

void IdentifyT::Simplify(ContextT &Context)
{
	Subject->Simplify(Context);
	Criteria->Simplify(Context);
	
	auto CriteriaGroup = Criteria.As<GroupT>();
	if (!CriteriaGroup) ERROR;
	std::string DestName;
	AtomT DestVariantClassAtom;
	for (auto &Pair : **CriteriaGroup)
	{
		if (DestVariantClassAtom) ERROR; // Max 1 in class group
		DestName = Pair.first;
		DestVariantClassAtom = Pair.second;
	}
	if (!DestVariantClassAtom) ERROR; // Min 1 in class group
	auto DestVariantClass = DestVariantClassAtom.As<VariantClassT>();
	
	if (auto Variant = Subject.As<VariantValueT>())
	{
		if (Variant->Class == DestVariantClass) // TODO Is this cool?
		{
			if (Body)
			{
				{
					auto BodyGroup = Body.As<GroupT>();
					if (!BodyGroup) ERROR;
					auto Dest = BodyGroup->AccessElement(Context, DestName);
					auto DestAssignable = Dest.As<ValueT>();
					DestAssignable->Assign(Context, Variant->Data);
				}
				Body->Simplify(Context);
			}
		}
		else 
		{
			if (Else)
			{
				Assert(Else.As<GroupT>());
				Else->Simplify(Context);
			}
		}
	}
	else
	{
		auto IfBlock = llvm::BasicBlock::Create(Context.LLVM, "", (*Context.Block)->getParent());
		auto ElseBlock = llvm::BasicBlock::Create(Context.LLVM, "", (*Context.Block)->getParent());
		auto EndBlock = llvm::BasicBlock::Create(Context.LLVM, "", (*Context.Block)->getParent());
		
		{
			auto Variable = Subject.As<VariableT>();
			Assert(Variable);
			if (Variable->Initialized.Get()) ERROR;
			llvm::BranchInst::Create(
				IfBlock, 
				ElseBlock, 
				new llvm::ICmpInst(
					**Context.Block, 
					llvm::CmpInst::ICMP_EQ, 
					GetElementPointer(Context, Variable->Target, 0, *Context.Block),
					DestVariantClass->GenerateLLVMLoadID(Context)),
				*Context.Block);
		}
		
		if (Body)
		{
			auto IfContext = Context;
			make_shared(IfContext.Block, IfBlock);
			
			auto Variable = new VariableT(Context.Position);
			Variable->Type = DestVariantClass->BaseType;
			auto DestType = Variable->Type.As<LLVMLoadableTypeT>();
			Variable->Target = new llvm::BitCastInst(
				GetElementPointer(IfContext, Variable->Target, 1, *Context.Block), 
				DestType->GenerateLLVMType(Context),
				"",
				*IfContext.Block);
			Variable->Initialized.Set(Context);
			
			auto BodyGroup = Body.As<GroupT>();
			if (!BodyGroup) ERROR;
			auto Resolution = BodyGroup->AccessElement(Context, DestName);
			auto ResolutionAssignable = Resolution.As<ValueT>();
			ResolutionAssignable->Assign(Context, Variable);
			
			BranchValues(Context, Context.Scope);
			Body->Simplify(IfContext);
			UnbranchValues(Context, Context.Scope);
			
			llvm::BranchInst::Create(EndBlock, *IfContext.Block);
		}
		
		if (Else)
		{
			auto ElseContext = Context;
			make_shared(ElseContext.Block, ElseBlock);
			
			BranchValues(Context, Context.Scope);
			Else->Simplify(ElseContext);
			UnbranchValues(Context, Context.Scope);
			
			llvm::BranchInst::Create(EndBlock, *ElseContext.Block);
		}
		
		*Context.Block = EndBlock;
	}
}

}

