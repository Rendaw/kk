/*
Simplify(Context) -> Generates llvm/header file/doc/whatever + replaces self with new item or nullptr
All statements should simplify to nullptr
*/

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
inline llvm::Type *LLVMInt(unsigned int Size) { return llvm::IntegerType::get(llvm::getGlobalContext(), Size); }
struct ContextT
{
	llvm::Context *LLVM;
	llvm::Module *Module;
	llvm::BasicBlock *Block;
};

struct NucleusT
{
	virtual ~NucleusT(void) {}
	virtual NucleusT *Simplify(ContextT Context) { return this; }
};

struct AtomT : ShareableT
{
	unique_ptr<NucleusT> Nucleus;
	
	void Replace(NucleusT *New) { Nucleus.reset(New); }
	operator bool(void) const { return Nucleus; }
	NucleusT *operator *(void) { return Nucleus; }
	NucleusT const *operator *(void) const { return Nucleus; }
	
	NucleusT *Simplify(ContextT Context) { Nucleus.replace(Nucleus->Simplify(Context)); }
};

typedef SharedPointerT<AtomT> SingleT;
typedef std::list<SharedPointerT<AtomT>> MultipleT;

// Interfaces
struct RealBaseT : NucleusT
{
	virtual void Allocate(ContextT Context, AllocExistenceT Existence, AtomT *Other) = 0;
	virtual void Assign(ContextT Context, AtomT *Value) = 0;
	virtual llvm::Value *GenerateLoad(llvm::BasicBlock *Block) = 0;
};

// Basic types + values
struct IntTypeT
{
};

template <typename BaseT> ConstantBaseT : RealBaseT
{
	BaseT Value;
	
	bool Assigned = false;
	
	void Allocate(ContextT Context, AllocExistenceT Existence, AtomT *Other) override
	{
		if (Existence == AllocExistenceT::Auto) 
			Other->Replace(Clone());
		else if (Existence == AllocExistenceT::Dynamic) 
			Other->Replace(new DynamicT(Context, false, Type()));
		else if (Existence == AllocExistenceT::Static) 
			Other->Replace(new DynamicT(Context, true, Type()));
	}
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto Value = dynamic_cast<decltype(this)>(Value);
		if (!Value) COMPILEERROR;
		Value = Value->Value;
		Assigned = true;
	}
};
// TODO
// Put generateload in type?
// Make intT typedef of constantbaset
// Clone + type functions in ConstantBaseT
// Make type a ConstantBaseT template argument

struct IntT : RealBaseT
{
	int Value;
	
	void Allocate(ContextT Context, AllocExistenceT Existence, AtomT *Other) override
	{
		if (Existence == AllocExistenceT::Auto) 
			Other->Replace(Clone());
		else if (Existence == AllocExistenceT::Dynamic) 
			Other->Replace(new DynamicT(Context, false, Type()));
		else if (Existence == AllocExistenceT::Static) 
			Other->Replace(new DynamicT(Context, true, Type()));
	}
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto Value = dynamic_cast<IntT *>(Value);
		if (!Value) COMPILEERROR;
		Value = Value->Value;
	}
	
	llvm::Value *GenerateLoad(ContextT Context) override
		{ return llvm::ConstantInt::get(LLVMInt(32), Value, true); }
};

// Dynamic
struct DynamicT : RealBaseT
{
	bool const Static;
	SingleT Type;
	llvm::Value *Target;
	
	// State
	bool Assigned = false;
	
	void Allocate(ContextT Context, AllocExistenceT Existence, AtomT *Other) override
	{
		bool Static = this->Static;
		if (Existence == AllocExistenceT::Dynamic) Static = false;
		else if (Existence == AllocExistenceT::Static) Static = true;
		Other->Replace(new DynamicT(Context, Static, Type));
	}

	void Assign(ContextT Context, AtomT *Value) override
	{
		if (Static && Assigned) COMPILEERROR;
		if (!Type->Equals(Value->Type())) COMPILEERROR;
		auto Value = dynamic_cast<RealBaseT *>(Value);
		new llvm::StoreInst(Value->GenerateLoad(), Target, Context.Block);
		Assigned = true;
	}
	
	llvm::Value *GenerateLoad(ContextT Context) override
		{ return new llvm::LoadInst(Target, "", Context.Block); }
		
	DynamicT(ContextT Context, bool Static, AtomT *Type, llvm::Value *Target = nullptr) : Static(Static), Type(Type), Target(Target)
	{
		if (!Target)
		{
			auto Type = dynamic_cast<TypeBaseT *>(*Type);
			Target = new llvm::AllocaInst(Type->GenerateType(), "", Block);
		}
	}
};

// Record
struct RecordT : RealBaseT
{
	bool Struct;
	MultipleT Statements;
	
	std::vector<std::pair<std::string, SingleT>>> Elements;
	
	llvm::Value *StructTarget;
	
	NucleusT *Simplify(ContextT Context)
	{
		for (auto &Statement : Statements)
			Statement->Simplify(Context);
		return this;
	}
	
	void Allocate(ContextT Context, AllocExistenceT Existence, AtomT *Other)
	{
		auto Record = new RecordT(Struct, {});
		if (Existence != AllocExistenceT::Auto) COMPILEERROR;
		for (auto &Element : Elements)
		{
			auto OtherElement = Record->Add(Element.first);
			Element->second->Allocate(Context, Existence, OtherElement);
		}
	}
	
	void Assign(ContextT Context, AtomT *Value)
	{
		auto Record = dynamic_cast<RecordT *>(*Value);
		if (Existence != AllocExistenceT::Auto) COMPILEERROR;
		for (auto &Element : Elements)
		{
			auto OtherElement = Record->Get(Element.first);
			Element->second->Assign(Context, OtherElement);
		}
	}
	
	llvm::Value *GenerateLoad(llvm::BasicBlock *Block) 
	{
		assert(Struct);
	}
	
	RecordT(bool Struct, std::list<SingleT> Statements) : Struct(Struct), Statements(Statements.begin(), Statements.end()) { }
};

// Assignment + access
struct AssignmentT : NucleusT
{
	AllocExistenceT Existence;
	SingleT Target;
	SingleT Value;
	
	NucleusT *Simplify(ContextT Context) override
	{
		Target->Simplify(Context);
		Value->Simplify(Context);
		auto Value = dynamic_cast<RealBaseT *>(*Value);
		if (dynamic_cast<UndefinedT *>(*Target))
			Value->Allocate(Context, Target);
		auto Target = dynamic_cast<RealBaseT *>(*Target);
		Target->Assign(Context, this->Value);
		return nullptr;
	}
};

// Functions + function statements
// Inter-module

// Go go go!
int main(int argc, char **argv)
{
	return 0;
}
