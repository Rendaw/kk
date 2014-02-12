/*
Simplify(Context) -> GenerateLLVMs llvm/header file/doc/whatever + replaces self with new item or nullptr
All statements should simplify to nullptr
*/

/*
Records just are group names for variables unless 1. assigned to new name.
Records are untyped when created explicitly.  Naming and use in a function types a record.  Any record type (including untyped records) can assign from an untyped record.  Struct records have two methods: LLVMLoad, and LLVMTryLoad;


Untyped record
refine -> struct with llvm alloc
implement 

new (type) x = add x to scope, allocate type, otherwise determine on first assignment

---
Okay, I got it now
All reals are untyped until 1. assigned (autodetermination), 2. cast (receive cast type)
Values are only copied on assignment (allocation) - otherwise they are references or temporaries
Therefore
	1. Assigning new val with a struct type creates a struct, assigns from group (with same type)
	2. Assigning old val with a different struct type creates an error
	3. Passing a record type assigns struct type, generates struct
	
---- 
Okay, really got it now
All atoms have two types, internal and affective
Internal is always generated on demand through autodetermination
Affective is usually null

-----
Okay, for serious really got it now

declare [TYPE] NAME -- resolves to reference to declared item
	if no type, undefined, else type->alloc 
simplify does (alloc for structs should alloca, same for dynamics I think, unless context is struct then use parent struct)
element/index for undefined values is an error
in assignment statement
	simplify target + value
	ALT 1
		if undefined, target->replace(value->type->alloc)
		simplify target again
	ALT 2
		if undefined
			if value typed
				target->replace(value)
			else target->replace(value->type->alloc) + simplify?
	target->assign(value)

in value->assign
	if target has type, make sure they are equal
	otherwise  ...? need type to determine that they are even records... maybe not, just cast to record and go for it

pass to function sets type + simplifies before llvmload or whatever

hint: make dynamic functions on interfaces static so they can't be individually reassigned

NG - aliasing errors in assignments

-------
Option 2
reference is a type annotation (int ref, dyn int ref, etc)

atoms have type automatically, no assigned type ever
declare [TYPE] [NAME]
allocas on simplify
in assignment statement
	simplify target + value
	if target undefined, target->replace(value->type->alloc)
	target->assign(value)
	
in value->assign
	if 

NG - mashes with untyped funtion bodies

----------------
FINAL I GOT IT

atoms have no type automatically
declare [TYPE] [NAME] - allocates var
assignments allowed from somewhat dissimilar types
assignment of untyped uses value type or forces value type autodetermination
assignment of typed can use compatible typed or untyped values
alloca on simplify
strict [TYPE] makes a strict copy of TYPE - value of type can only be assigned in whole from same type
INTERFACE types must be linear - heap allocated or no, there will only be one name (and therefore one pointer) ever - assignment using memcpy or pointer swap is equivalent
coerce (expr) - no type, but ignores strictness in assignment, unsimplified
function call sets type if not set, checks compatibility otherwise

TODO
remove allocate
cleanup
declare
strict
coerce

coerce only works on simple types

---------------
OKAY ACTUAL FINAL I GOT IT

cast - sets type of untyped (recorddef and literals)
function-level assignment
	target type = value
record assignment (simple)
	name = value
recorddef vs record
simple assignment
	record->add(name, value)
function assignment
	if target undefined
		if type
			target->replace(type->allocate)
		else target->replace(value->type->allocate)
	target->assign(value)

simple = sub: subassign and assign
	
*/

/////
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

// Basics
inline llvm::Type *GenerateLLVMInt(unsigned int Size) { return llvm::IntegerType::get(llvm::getGlobalContext(), Size); }
struct ContextT
{
	llvm::Context *LLVM;
	llvm::Module *Module;
	llvm::BasicBlock *Block;
	
	AtomT *Context;
	
	ContextT WithContext(AtomT *Context)
		{ return {LLVM, Module, Block, Context}; }
		
	ContextT WithBlock(llvm::Block *Block)
		{ return {LLVM, Module, Block, Struct}; }
};


//////////////////
struct SingleT;
struct AtomT : ShareableT
{
	AtomT *Context = nullptr;
  
	std::list<SingleT *> References;
	void Replace(AtomT *Other);
	
	virtual AtomT *Clone(void) const = 0;
	
	virtual AtomT *Simplify(ContextT Context) { return this; }
	
	SingleT Type;
	AtomT *GetType(void) { return Type; }
	virtual void SetType(AtomT *NewType) = 0;
	virtual void DetermineType(void) {}
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

//////////////////
/*struct NucleusT;
struct AtomT : ShareableT
{
	unique_ptr<NucleusT> Nucleus;
	
	void Replace(NucleusT *New) { Nucleus.reset(New); }
	operator bool(void) const { return Nucleus; }
	NucleusT *operator *(void) { return Nucleus; }
	NucleusT const *operator *(void) const { return Nucleus; }
	typedef <DestT> DestT *To(void) { return dynamic_cast<DestT *>(Nucleus.get()); }
	
	AtomT *Clone(void) { return new AtomT(Nucleus->Clone()); }
	void Simplify(ContextT Context) { Nucleus.replace(Nucleus->Simplify(Context)); }
	NucleusT *GetType(void) { Nucleus->GetType(); }
};

typedef SharedPointerT<AtomT> SingleT;
typedef std::list<SharedPointerT<AtomT>> MultipleT;

struct NucleusT
{
	virtual ~NucleusT(void) {}
	
	virtual NucleusT *Clone(void) const = 0;
	
	virtual NucleusT *Simplify(ContextT Context) { return this; }
	
	SingleT Type;
	NucleusT *GetType(void) { if (!Type) InitializeType(); return Type; }
	virtual void InitializeType(void) {}
	
	NucleusT(void) {}
	NucleusT(NucleusT *Type) : Type(Type) {}
};*/

// Interfaces
struct RealBaseT
{
	virtual void Assign(ContextT Context, AtomT *Value) = 0;
};

struct TypeBaseT
{
	virtual AtomT *Allocate(ContextT Context) = 0;
	virtual llvm::Type *GenerateLLVMType(ContextT Context) = 0;
	virtual void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types) = 0;
};

enum struct ExistenceT
{
	Constant,
	Static,
	Dynamic
};

struct SimpleTypeBaseT : TypeBaseT
{
	ExistenceT Existence;
	
	SimpleTypeBaseT(ExistenceT Existence) : Existence(Existence) {}
};

struct SimpleBaseT : RealBaseT
{
	virtual llvm::Value *GenerateLLVMLoad(ContextT Context) = 0;
};

// Basic types + values
struct UndefinedT : AtomT
{
	AtomT *Clone(void) const { return new UndefinedT; }
	void SetType(void) { COMPILEERROR; }
};

struct TypeTypeT : AtomT
{
	AtomT *Clone(void) const { return new TypeTypeT; }
	void SetType(AtomT *NewType) { COMPILEERROR; }
};

template <typename ConstT> struct SimpleTypeCommonT : AtomT, SimpleTypeBaseT
{
	AtomT *Allocate(ContextT Context) override
	{
		if (Existence == ExistenceT::Constant) 
			return new ConstT;
		else if ((Existence == ExistenceT::Dynamic) || (Existence == ExistenceT::Static))
			return new DynamicT(Clone(), new llvm::AllocaInst(GenerateLLVMType(), "", Context.Block));
		else { assert(0); return nullptr; }
	}
	
	void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types) override
	{
		Types.push_back(GenerateLLVMType(Context));
	}
	
	void SetType(AtomT *NewType) { DetermineType();  if (!Type->Equals(NewType)) COMPILEERROR; }
	
	void DetermineType(void) { if (Type) return; Type = new TypeTypeT; }
	
	using SimpleTypeBaseT::SimpleTypeBaseT;
};

struct IntTypeT : SimpleTypeCommonT<IntT>
{
	AtomT *Clone(void) const override { return new IntTypeT(Existence); }
	
	llvm::Type *GenerateLLVMType(ContextT Context) override
	{
		return llvm::IntegerType::get(Context.LLVM, 32);
	}
	
	IntTypeT(ExistenceT Existence) : SimpleTypeCommonT<IntT>(Existence) {}
};

template <typename BaseT, typename TypeT> ConstantCommonT : AtomT, SimpleBaseT
{
	BaseT Value;
	
	// State
	bool Assigned;
	
	void InitializeType(void) override
	{
		Type = new TypeT(ExistenceT::Constant);
	}
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto Value = dynamic_cast<decltype(this)>(*Value);
		if (!Value) COMPILEERROR;
		Value = Value->Value;
		Assigned = true;
	}
	
	ConstantCommonT(void) : Assigned(false) {}
	ConstantCommonT(BaseT Value, bool Assigned) : Value(Value), Assigned(Assigned) {}
};

struct IntT : ConstantCommonT<int, IntTypeT>
{
	AtomT *Clone(void) const override { return new IntT(Value, Assigned); }
	  
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
		{ assert(Assigned); return llvm::ConstantInt::get(GenerateLLVMInt(32), Value, true); }
		
	IntT(void) {}
	IntT(int Value, bool Assigned = true) : ConstantCommonT<int, IntTypeT>(Value, Assigned) {}
};

// Dynamic
struct DynamicT : AtomT, RealBaseT
{
	// TODO Struct base
	llvm::Value *Target;
	
	// State
	bool Assigned = false;
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		if ((Type->Existence == ExistenceT::Static) && Assigned) COMPILEERROR;
		if (!Type->Equals(Value->GetType())) COMPILEERROR;
		auto Value = dynamic_cast<RealBaseT *>(Value);
		new llvm::StoreInst(Value->GenerateLLVMLoad(), Target, Context.Block);
		Assigned = true;
	}
	
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
		{ assert(Assigned); return new llvm::LoadInst(Target, "", Context.Block); }
		
	DynamicT(AtomT *Type, llvm::Value *Target = nullptr) : AtomT(Type), Target(Target) {}
};

// Record
struct RecordT : AtomT, RealBaseT, TypeBaseT
{
	// Structure
	bool Struct;
	MultipleT Statements;
	
	// State
	std::vector<std::pair<std::string, SingleT>> Elements;
	
	llvm::Type *StructType;
	llvm::Value *StructTarget;
	
	AtomT *Simplify(ContextT Context) override
	{
		for (auto &Statement : Statements)
			Statement->Simplify(Context);
		return this;
	}
	
	void InitializeType(void) override
	{
		auto Type = new RecordT(Struct);
		for (auto &Element : Elements)
		{
			Type->Add(Element.first, Element.second->Type()->Clone());
		}
		this->Type = new AtomT(Type);
	}
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto Record = dynamic_cast<RecordT *>(**Value);
		if (Struct)
		{
			llvm::Value *MemcpyOriginal = nullptr;
			if (Record->Struct)
			{
				// Make sure dynamic element names, count, and order match
				bool Matches = true;
				auto ThisElement = Elements.begin();
				auto ThatElement = Record->Elements.begin();
				while (ThisElement != Elements.end())
				{
					while ((ThisElement != Elements.end()) && !ThisElement->second->To<DynamicT>()) ++ThisElement;
					while ((ThatElement != Record->Elements.end()) && !ThatElement->second->To<DynamicT>()) ++ThatElement;
					if (ThatElement != Record->Elements.end()) ++ThatCount;
					if (ThisElement != Elements.end()) ++ThisCount;
					else continue;
				}
				for (auto &Element : Record->Elements)
				{
					if 
				}
				if (Record->StructMatches(GetType()))
				MemcpyOriginal = Record->GenerateLLVMLoad(Context);
			else if (
			auto *RecordValue = Record->GenerateLLVMLoad(Context);
			if (RecordValue)
			{
				assert(StructTarget);
				auto Dest = new llvm::BitCastInst(StructTarget, PointerType, "", Context.Block);
				auto Source = new llvm::BitCastInst(RecordValue, PointerType, "", Context.Block);
				new llvm
			}
			
		}
		// TODO struct - memcpy and skip dynamic elements if GenerateLLVMLoad returns a value
		for (auto &Element : Elements)
		{
			auto OtherElement = Record->Get(Element.first);
			Element.second->Assign(Context, OtherElement);
		}
	}
	
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{
		if (Struct)
		{
			assert(StructTarget);
			return StructTarget;
		}
		else
		{
		}
		// TODO Okay if not struct, if all const (make llvm::ConstantStruct)
		// return null if not all const, do traditional assignment
		assert(Struct);
		return new llvm::LoadInst(StructTarget, "", Context.Block);
	}

	AtomT *Allocate(ContextT Context) override
	{
		llvm::Value *StructTarget = nullptr;
		ContextT SubContext;
		if (Struct) 
		{
			StructTarget = new llvm::AllocaInst(GenerateLLVMType(), "", Context.Block);
			SubContext = Context.WithStruct(StructTarget);
		}
		auto Record = new RecordT(Struct, StructTarget);
		if (Struct && !StructType)
		for (auto &Element : Elements)
		{
			if (Type = dynamic_cast<TypeBaseT *>(**Element.second))
				Record->Add(Element.first, Type->Allocate(SubContext, Existence));
			else Record->Add(Element.first, Element.second->Clone());
		}
		return Record;
	}
	
	llvm::Type *GenerateLLVMType(ContextT Context)
	{
		assert(Struct);
		if (!StructType)
		{
			std::vector<llvm::Type *> Types;
			for (auto &Element : Elements)
				if (Type = dynamic_cast<TypeBaseT *>(**Element.second))
					Type->GenerateLLVMTypes(Context, Types);
			StructType = llvm::StructType::create(Types);
		}
	}
	
	void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types)
	{
		if (Struct) Types.push_back(GenerateLLVMType(Context));
		else 
		{
			for (auto &Element : Elements)
			{
				if (Type = dynamic_cast<TypeBaseT *>(**Element.second))
					Type->GenerateTypes(Context, Types);
			}
		}
	}
	
	void Add(std::string const &Name, AtomT *Item) 
	{
		if (Type) COMPILEERROR; // Can't add elements after type determination
		for (auto &Element : Elements) assert(Element.first() != Name);
		Elements.emplace_back(Name, Item);
	}
	
	AtomT *Get(std::string const &Name)
	{
		for (auto &Element : Elements) if (Element.first() == Name) return Element.second();
		return nullptr;
	}
	
	RecordT(bool Struct, std::list<SingleT> Statements) : Struct(Struct), Statements(Statements.begin(), Statements.end()), StructType(nullptr), StructTarget(nullptr) { }
	RecordT(bool Struct, llvm::Value *StructTarget) : Struct(Struct), StructType(nullptr), StructTarget(StructTarget) {}
};

// Assignment + access
struct AssignmentT : AtomT
{
	SingleT Target;
	SingleT Type;
	SingleT Value;
	
	void Simplify(ContextT Context) override
	{
		Target->Simplify(Context);
		if (Type)
			Type->Simplify(Context);
		Value->Simplify(Context);
		if (Target->To<UndefinedT>()) 
		{
			if (Type
			if (!Value->Type)
			{
				Value->DetermineType();
				Value->Simplify(Context);
			}
			Target->Replace(Value->Type->To<TypeBaseT>()->Allocate(Context));
		}
		auto Target = Target->To<RealBaseT>();
		Target->Assign(Context, Value);
	}
};

// Functions + function statements
// Inter-module

// Go go go!
int main(int argc, char **argv)
{
	// Test const rec assign from const rec
	// Test struct assign from const
	// struct assign from struct
	// value assign from struct
	// struct value assign from value
	return 0;
}
