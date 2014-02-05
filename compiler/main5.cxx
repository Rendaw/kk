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

struct NucleusT;
struct AtomT : ShareableT
{
	unique_ptr<NucleusT> Nucleus;
	
	void Replace(NucleusT *New) { Nucleus.reset(New); }
	operator bool(void) const { return Nucleus; }
	NucleusT *operator *(void) { return Nucleus; }
	NucleusT const *operator *(void) const { return Nucleus; }
	
	void Simplify(ContextT Context) { Nucleus.replace(Nucleus->Simplify(Context)); }
	AtomT *GetType(void) { Nucleus->GetType(); }
};

typedef SharedPointerT<AtomT> SingleT;
typedef std::list<SharedPointerT<AtomT>> MultipleT;

struct NucleusT
{
	virtual ~NucleusT(void) {}
	
	virtual NucleusT *Simplify(ContextT Context) { return this; }
	
	SingleT Type;
	AtomT *GetType(void) { if (!Type) InitializeType(); return Type; }
	virtual void InitializeType(void) {}
};

// Interfaces
struct RealBaseT
{
	virtual void Assign(ContextT Context, AtomT *Value) = 0;
	virtual llvm::Value *GenerateLoad(ContextT Context) = 0;
};

struct TypeBaseT
{
	virtual NucleusT *Allocate(ContextT Context, AllocExistenceT Existence) = 0;
};

struct SimpleTypeBaseT : TypeBaseT
{
	ExistenceT Existence;
	
	SimpleTypeBaseT(ExistenceT Existence) : Existence(Existence) {}
};

// Basic types + values
template <typename ConstT> struct SimpleTypeCommonT : NucleusT, SimpleTypeBaseT
{
	AtomT *Allocate(ContextT Context, AllocExistenceT AllocExistence) override
	{
		auto EffectiveExistence = Existence;
		if (AllocExistence == AllocExistenceT::Dynamic) EffectiveExistence = ExistenceT::Dynamic;
		if (AllocExistence == AllocExistenceT::Static) EffectiveExistence = ExistenceT::Static;
		if (EffectiveExistence == ExistenceT::Constant) 
			return new ConstT;
		else if ((EffectiveExistence == ExistenceT::Dynamic) || (EffectiveExistence == ExistenceT::Static))
			return new DynamicT(Context, false, new decltype(*this)(EffectiveExistence));
		else { assert(0); return nullptr; }
	}
	
	using SimpleTypeBaseT::SimpleTypeBaseT;
};

typedef SimpleTypeCommonT<IntT> IntTypeT;

template <typename BaseT, typename TypeT> ConstantCommonT : NucleusT, RealBaseT
{
	BaseT Value;
	
	bool Assigned = false;
	
	void InitializeType(void) override
	{
		Type = new TypeT(ExistenceT::Constant);
	}
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto Value = dynamic_cast<decltype(this)>(Value);
		if (!Value) COMPILEERROR;
		Value = Value->Value;
		Assigned = true;
	}
	
};

struct IntT : ConstantCommonT<int, IntTypeT>
{
	int Value;
	
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
struct DynamicT : NucleusT, RealBaseT
{
	llvm::Value *Target;
	
	// State
	bool Assigned = false;
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		if ((Type->Existence == ExistenceT::Static) && Assigned) COMPILEERROR;
		if (!Type->Equals(Value->GetType())) COMPILEERROR;
		auto Value = dynamic_cast<RealBaseT *>(Value);
		new llvm::StoreInst(Value->GenerateLoad(), Target, Context.Block);
		Assigned = true;
	}
	
	llvm::Value *GenerateLoad(ContextT Context) override
		{ return new llvm::LoadInst(Target, "", Context.Block); }
		
	DynamicT(ContextT Context, AtomT *Type, llvm::Value *Target = nullptr) : Static(Type->Existence == ExistenceT::Static), Target(Target)
	{
		this->Type = Type;
		if (!Target)
		{
			auto Type = dynamic_cast<TypeBaseT *>(**Type);
			Target = new llvm::AllocaInst(Type->GenerateType(), "", Block);
		}
	}
};

// Record
struct RecordT : NucleusT, RealBaseT, TypeBaseT
{
	// Structure
	bool Struct;
	MultipleT Statements;
	
	// State
	std::vector<std::pair<std::string, SingleT>>> Elements;
	
	llvm::Value *StructTarget;
	
	NucleusT *Simplify(ContextT Context) override
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
		for (auto &Element : Elements)
		{
			auto OtherElement = Record->Get(Element.first);
			Element.second->Assign(Context, OtherElement);
		}
	}
	
	llvm::Value *GenerateLoad(ContextT Context) override
	{
		assert(Struct);
		return new llvm::LoadInst(StructTarget, "", Context.Block);
	}

	NucleusT *Allocate(ContextT Context, AllocExistenceT Existence) override
	{
		auto Record = new RecordT(Struct, {});
		for (auto &Element : Elements)
		{
			if (Type = dynamic_cast<TypeBaseT *>(**Element.second))
				Record->Add(Element.first, Type->Allocate(Context, Existence));
			else Record->Add(Element.first, Element.second->Clone());
		}
		return Record;
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
		if (Target->To<UndefinedT>()) 
		{
			auto Type = Value->Type();
			Target->Replace(Type->Allocate(Context, Existence));
		}
		auto Target = Target->To<RealBaseT>();
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
