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
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define COMPILEERROR assert(0)

struct SturctParentIndexT : ShareableT
{
	// I would just not copy contextT when recursing, but I can't mix r-value references and references in the same function because c++ sucks
	size_t Count;
};

struct AtomT;
struct ContextT
{
	llvm::LLVMContext &LLVM;
	llvm::Module *Module;
	
	llvm::Function *Memcpy;
	
	llvm::BasicBlock *Block;
	AtomT *Scope;
	
	// Allocation
	AtomT *StructParent; // Optional
	SharedPointerT<StructParentIndexT> StructParentIndex;
	
	// Assignment
	bool Memcopied;
	
	ContextT(
		llvm::LLVMContext &LLVM,
		llvm::Module *Module,
		llvm::Function *Memcpy,
		llvm::BasicBlock *Block,
		AtomT *Scope,
		AtomT *StructParent = 0,
		SharedPointerT<StructParentIndexT> StructParentIndex = nullptr,
		bool Memcopied = false
	) :
		LLVM(LLVM),
		Module(Module),
		Memcpy(Memcpy),
		Block(Block),
		Scope(Scope),
		StructParent(StructParent),
		StructParentIndex(StructParentIndex),
		Memcopied(Memcopied)
	{
	}
	
	#define DERIVE {LLVM, Module, Memcpy, Block, Scope, StructParent, StructParentIndex, Memcopied}
	ContextT WithBlock(llvm::BasicBlock *Block)
		{ return DERIVE; }

	ContextT WithScope(AtomT *Scope)
		{ return DERIVE; }
		
	ContextT WithAllocStruct(AtomT *StructParent, SharedPointerT<StructParentIndexT> StructParentIndex)
		{ return DERIVE; }
	ContextT WithoutAllocStruct(void)
	{
		AtomT *StructParent = nullptr;
		SharedPointerT<StructParentIndexT> StructParentIndex;
		return DERIVE;
	}
	
	ContextT WithAssignMemcopied(bool Memcopied)
		{ return DERIVED; }
	#undef DERIVE
};

struct SingleT
{
	private:
		mutable SharedPointerT<AtomT> Internal; // mutable because the standard library is insane
		void Clean(void);
	public:

	SingleT(void) {}
	SingleT(AtomT *Base);
	SingleT(SingleT &Base);
	SingleT(SingleT const &Base);
	SingleT(SingleT &&Base);

	~SingleT(void) { Clean(); }

	SingleT &operator =(AtomT *Base);
	SingleT &operator =(SingleT &Base);
	SingleT &operator =(SingleT &&Base);

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
	
struct AtomT : ShareableT
{
	template <typename TargetT> TargetT *To(void) { return dynamic_cast<TargetT *>(this); }
	template <typename TargetT> TargetT const *To(void) const { return dynamic_cast<TargetT const *>(this); }
  
	std::list<SingleT *> References;
	void Replace(AtomT *Other)
	{
		SharedPointerT<AtomT> Keep(this);
		auto References = this->References;
		for (auto &Reference : References)
			if (Reference) *Reference = Other;
		References.clear();
		assert(Keep.Count() == 1);
	}
	
	virtual AtomT *Clone(void) const { COMPILEERROR; } // Should be implemented for constants and non-internal atoms, but otherwise invalid I think. (record cloning and function refinement)
	
	virtual void Simplify(ContextT Context) {}
	
	SingleT Type;
	AtomT *GetType(void) { if (!Type) Autotype(); return Type; }
	virtual void Autotype(void) {}
	
	AtomT(void) {}
	AtomT(AtomT *Type) : Type(Type) {}
};

void SingleT::Clean(void) 
{ 
	if (Internal) 
		Internal->References.erase(
			std::find(
				Internal->References.begin(), 
				Internal->References.end(), 
				this)); 
}
SingleT::SingleT(AtomT *Base) : Internal(Base) 
	{ if (Internal) Internal->References.push_back(this); }
SingleT::SingleT(SingleT &Base) : Internal(Base.Internal) 
	{ if (Internal) Internal->References.push_back(this); }
SingleT::SingleT(SingleT const &Base) : Internal(Base.Internal) 
	{ if (Internal) Internal->References.push_back(this); }
SingleT::SingleT(SingleT &&Base) : Internal(Base.Internal) 
	{ if (Internal) Internal->References.push_back(this); }
SingleT &SingleT::operator =(AtomT *Base) 
{ 
	Clean(); 
	Internal = Base; 
	if (Internal) Internal->References.push_back(this); 
	return *this; 
}

SingleT &SingleT::operator =(SingleT &Base) 
{ 
	Clean(); 
	Internal = Base; 
	if (Internal) Internal->References.push_back(this); 
	return *this; 
}

SingleT &SingleT::operator =(SingleT &&Base) 
{ 
	Clean(); 
	Internal = Base; 
	if (Internal) Internal->References.push_back(this); 
	return *this; 
}

typedef std::list<SingleT> MultipleT;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Interfaces
struct ValueBaseT
{
	virtual void Assign(ContextT Context, AtomT *Value) = 0;
	virtual llvm::Value *GenerateLLVMLoad(ContextT Context) = 0;
};

struct TypeBaseT
{
	virtual AtomT *Allocate(ContextT Context) = 0;
	virtual llvm::Type *GenerateLLVMType(ContextT Context) { COMPILEERROR; return nullptr; }
	virtual void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types) { COMPILEERROR; }
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Basics
struct UndefinedT : AtomT
{
	AtomT *Clone(void) const { return new UndefinedT; }
};

struct TypeTypeT : AtomT
{
	AtomT *Clone(void) const { return new TypeTypeT; }
};

struct SubAssignmentT : AtomT
{
	SingleT Name;
	SingleT Value;
	
	AtomT *Clone(void) const override { return new SubAssignmentT(Name->Clone(), Value->Clone()); }
	
	void Simplify(ContextT Context) override { Name->Simplify(Context); Value->Simplify(Context); }
	
	SubAssignmentT(AtomT *Name, AtomT *Value) : Name(Name), Value(Value) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Dynamic
struct DynamicT : AtomT, ValueBaseT
{
	// State
	llvm::Value *Target;
	
	bool Assigned = false;
	
	AtomT *Clone(void) const override { COMPILEERROR; return nullptr; }
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		if (Context.Memcopied) 
		{
			Assigned = true;
			return;
		}
		
		auto Type = this->Type->To<SimpleTypeBaseT>();
		if ((Type->Existence == ExistenceT::Static) && Assigned) COMPILEERROR;
		auto SimpleValue = Value->To<ValueBaseT>();
		new llvm::StoreInst(SimpleValue->GenerateLLVMLoad(Context), Target, Context.Block);
		Assigned = true;
	}
	
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{ 
		assert(Assigned); 
		return new llvm::LoadInst(Target, "", Context.Block); 
	}
		
	DynamicT(AtomT *Type, llvm::Value *Target) : AtomT(Type), Target(Target) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Constants
template <typename ConstT> struct SimpleTypeCommonT : AtomT, SimpleTypeBaseT
{
	AtomT *Allocate(ContextT Context) override
	{
		if (Existence == ExistenceT::Constant) 
			return new ConstT(Clone());
		else if ((Existence == ExistenceT::Dynamic) || (Existence == ExistenceT::Static))
		{
			llvm::Value *Target;
			if (Context.StructParent)
			{
				auto StructParent = Context.StructParent->To<ValueBaseT>();
				Target = new llvm::GetElementPtrInst(StructParent->GenerateLLVMLoad(Context), Context.StructParentIndex->Count++);
			}
			else Target = new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.Block);
			return new DynamicT(Clone(), Target);
		}
		else { assert(0); return nullptr; }
	}
	
	void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types) override
	{
		auto Type = GenerateLLVMType(Context);
		if (Type) Types.push_back(Type); 
	}
	
	virtual llvm::Type *GenerateLLVMTypeInternal(ContextT Context) = 0;
	
	llvm::Type *GenerateLLVMType(ContextT Context) override
	{
		if ((Existence != ExistenceT::Dynamic) && (Existence != ExistenceT::Dynamic)) return nullptr;
		return GenerateLLVMTypeInternal(Context);
	}
	
	void Autotype(void) { Type = new TypeTypeT; }
	
	SimpleTypeCommonT(ExistenceT Existence) : SimpleTypeBaseT(Existence) {}
};

struct IntT;
struct IntTypeT : SimpleTypeCommonT<IntT>
{
	AtomT *Clone(void) const override { return new IntTypeT(Existence); }
	
	llvm::Type *GenerateLLVMTypeInternal(ContextT Context) override
		{ return llvm::IntegerType::get(Context.LLVM, 32); }
	
	IntTypeT(ExistenceT Existence) : SimpleTypeCommonT<IntT>(Existence) {}
};

struct StringT;
struct StringTypeT : SimpleTypeCommonT<StringT>
{
	AtomT *Clone(void) const override { return new StringTypeT(Existence); }
	
	llvm::Type *GenerateLLVMTypeInternal(ContextT Context) override 
		{ return llvm::PointerType::getUnqual(llvm::IntegerType::get(Context.LLVM, 8)); }
	
	StringTypeT(ExistenceT Existence) : SimpleTypeCommonT<StringT>(Existence) {}
};

template <typename BaseT, typename TypeT> struct ConstantCommonT : AtomT, ValueBaseT
{
	BaseT Value;
	
	// State
	bool Assigned;
	
	void Autotype(void) override
		{ Type = new TypeT(ExistenceT::Constant); }
	
	void Assign(ContextT Context, AtomT *Value) override
	{
		auto ConstValue = Value->To<typename std::remove_reference<decltype(*this)>::type>();
		this->Value = ConstValue->Value;
		Assigned = true;
	}
	
	ConstantCommonT(AtomT *Type) : AtomT(Type), Assigned(false) {}
	ConstantCommonT(AtomT *Type, BaseT Value, bool Assigned) : AtomT(Type), Value(Value), Assigned(Assigned) {}
};

struct IntT : ConstantCommonT<int, IntTypeT>
{
	AtomT *Clone(void) const override { return new IntT(Type ? Type->Clone() : nullptr, Value, Assigned); }
	  
	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{ 
		assert(Assigned); 
		auto Type = this->GetType()->To<SimpleTypeBaseT>();
		return llvm::ConstantInt::get(Type->GenerateLLVMType(Context), Value, true); 
	}
		
	IntT(AtomT *Type) : ConstantCommonT<int, IntTypeT>(Type) {}
	IntT(AtomT *Type, int Value, bool Assigned = true) : ConstantCommonT<int, IntTypeT>(Type, Value, Assigned) {}
};

struct StringT : ConstantCommonT<std::string, StringTypeT>
{
	AtomT *Clone(void) const override { return new StringT(Type ? Type->Clone() : nullptr, Value, Assigned); }
	  
	llvm::Value *GenerateLLVMLoad(ContextT Context) 
	{ 
		assert(Assigned); 
		return llvm::ConstantDataArray::getString(Context.LLVM, Value.c_str()); 
	}
		
	StringT(AtomT *Type) : ConstantCommonT<std::string, StringTypeT>(Type) {}
	StringT(AtomT *Type, std::string Value, bool Assigned = true) : ConstantCommonT<std::string, StringTypeT>(Type, Value, Assigned) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Record
struct RecordT : AtomT, ValueBaseT, TypeBaseT
{
	bool Struct;
	MultipleT Assignments; // Must be simple assignments
	
	// State
	struct PairT
	{
		std::string Name;
		SingleT Value;
		PairT(std::string Name, AtomT *Value) : Name(Name), Value(Value) {}
		PairT(PairT const &Other) : Name(Other.Name), Value(Other.Value) {}
	};
	std::vector<PairT> Elements;
	
	llvm::Type *StructTargetType;
	llvm::Value *StructTarget;
	
	void Add(std::string const &Name, AtomT *Value)
	{
		for (auto &Element : Elements) if (Element.Name == Name) COMPILEERROR;
		Elements.push_back(PairT(Name, Value));
	}
	
	AtomT *Get(std::string const &Name)
	{
		for (auto &Element : Elements) if (Element.Name == Name) return Element.Value;
		return nullptr;
	}
	
	AtomT *GetOrCreate(std::string const &Name)
	{
		for (auto &Element : Elements) if (Element.Name == Name) return Element.Value;
		auto Out = new UndefinedT;
		Add(Name, Out);
		return Out;
	}
	
	AtomT *GetIndex(size_t Index)
	{
		if (Index >= Elements.size()) return nullptr;
		return Elements[Index].Value;
	}
	
	AtomT *Clone(void) const override
	{
		auto Out = new RecordT(Struct, Type ? Type->Clone() : nullptr, {});
		for (auto &Assignment : Assignments) 
			if (Assignment) Out->Assignments.push_back(Assignment->Clone());
		for (auto &Element : Elements) 
			Out->Elements.push_back(PairT(Element.Name, Element.Value->Clone()));
		return Out;
	}
	
	void Simplify(ContextT Context) override
	{
		for (auto &Assignment : Assignments)
		{
			Assignment->Simplify(Context);
			auto SubAssignment = Assignment->To<SubAssignmentT>();
			auto Name = SubAssignment->Name->To<StringT>();
			Add(Name->Value, SubAssignment->Value);
			Assignment->Replace(nullptr);
		}
	}
	
	void Autotype(void) override
	{
		auto Type = new RecordT(Struct, nullptr, {});
		for (auto &Element : Elements)
			Type->Add(Element.Name, Element.Value->GetType());
		this->Type = Type;
	}

	void Assign(ContextT Context, AtomT *Value) override
	{
		auto RecordValue = Value->To<RecordT>();
		auto NewContext = Context;
		if (Struct && !NewContext.Memcopied)
		{
			auto Source = RecordValue->GenerateCustomLLVMLoad(Context, this);
			auto Dest = GenerateLLVMLoad(Context);
			if (Source && Dest)
			{
				std::vector<llvm::Value *> Params;
				Params.push_back(Dest);
				Params.push_back(Source);
				Params.push_back(llvm::ConstantExpr::getSizeOf(GenerateLLVMType(Context)));
				Params.push_back(llvm::ConstantInt::get(llvm::IntegerType::get(Context.LLVM, 32), 0, true); 
				Params.push_back(llvm::ConstantInt::get(llvm::IntegerType::get(Context.LLVM, 1), 0, true); 
				llvm::CallInst::Create(Context.Memcpy, Params, "", Context.Block);
				NewContext.Memcopied = true;
			}
		}
		for (auto &Element : Elements)
		{
			auto Source = RecordValue->Get(Element.Name);
			auto Dest = Element.Value->To<ValueBaseT>();
			Dest->Assign(NewContext, Source);
		}
	}
	
	llvm::Value *GenerateCustomLLVMLoad(ContextT Context, RecordT *Template)
	{
		// if dynamic order + count matches, StructTarget
		// if all template dynamic are in this and const, make const struct and return it
	}

	llvm::Value *GenerateLLVMLoad(ContextT Context) override
	{
		if (!Struct) COMPILEERROR;
		return StructTarget;
	}
	
	AtomT *Allocate(ContextT Context) override
	{
		auto Out = new RecordT(Struct, Clone(), {});
		
		ContextT NewContext = Context;
		if (Struct)
		{
			if (Context.StructParent)
				StructTarget = llvm::GetElementPtrInst::Create(Context.StructParent, Context.StructParentIndex.Count++);
			else StructTarget = new llvm::AllocaInst(GenerateLLVMType(Context), "", Context.Block);
			NewContext.StructParent = StructTarget;
			NewContext.StructParentIndex = new StructParentIndexT{0};
		}
		
		for (auto &Element : Elements)
		{
			if (auto Type = Element.Value->To<TypeBaseT>()) 
				Out->Add(Element.Name, Type->Allocate(NewContext));
			else Out->Add(Element.Name, Element.Value->Clone());
		}
		
		return Out;
	}

	llvm::Type *GenerateLLVMType(ContextT Context) override
	{
		if (!Struct) COMPILEERROR;
		if (!StructTargetType)
		{
			std::vector<llvm::Type *> Types;
			for (auto &Element : Elements)
			{
				auto Type = Element.Value->To<TypeBaseT>();
				if (Type) Type->GenerateTypes(Types);
			}
			StructTargetType = llvm::StructType::create(Types);
		}
		return StructTargetType;
	}
	
	void GenerateLLVMTypes(ContextT Context, std::vector<llvm::Type *> &Types) override
	{
		if (Struct) Types.push_back(GenerateLLVMType(Context));
		else
		{
			for (auto &Element : Elements)
			{
				auto Type = Element.Value->To<TypeBaseT>();
				if (Type) Type->GenerateTypes(Types);
			}
		}
	}
	
	RecordT(bool Struct, AtomT *Type, MultipleT const &Assignments) : AtomT(Type), Struct(Struct), Assignments(Assignments) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Glue
struct AssignmentT : AtomT
{
	SingleT Target;
	SingleT Type; // Optional
	SingleT Value;

	AtomT *Clone(void) const override { return new AssignmentT(Target->Clone(), Type->Clone(), Value->Clone()); }
	
	void Simplify(ContextT Context) override
	{
		Target->Simplify(Context);
		if (Type) Type->Simplify(Context);
		Value->Simplify(Context);
		if (Target->To<UndefinedT>())
		{
			if (Type) 
			{
				auto Type = this->Type->To<TypeBaseT>();
				Target->Replace(Type->Allocate(Context));
			}
			else 
			{
				auto Type = Value->GetType()->To<TypeBaseT>();
				Target->Replace(Type->Allocate(Context));
			}
		}
		auto Target = this->Target->To<ValueBaseT>();
		Target->Assign(Context, Value);
	}
	
	AssignmentT(AtomT *Target, AtomT *Type, AtomT *Value) : Target(Target), Type(Type), Value(Value) {}
};

struct ElementT : AtomT
{
	SingleT Base;
	SingleT Name;
	
	AtomT *Clone(void) const override { return new ElementT(Base ? Base->Clone() : nullptr, Name->Clone()); }
	
	void Simplify(ContextT Context) override
	{
		if (!Base) Base = Context.Scope;
		Base->Simplify(Context);
		auto Base = this->Base->To<RecordT>();
		Name->Simplify(Context);
		auto Name = this->Name->To<StringT>();
		Replace(Base->GetOrCreate(Name->Value));
	}
	
	ElementT(AtomT *Base, AtomT *Name) : Base(Base), Name(Name) {}
};

struct IndexT : AtomT
{
	SingleT Base;
	SingleT Index;
	
	AtomT *Clone(void) const override { return new IndexT(Base ? Base->Clone() : nullptr, Index->Clone()); }
	
	void Simplify(ContextT Context) override
	{
		if (!Base) Base = Context.Scope;
		Base->Simplify(Context);
		auto Base = this->Base->To<RecordT>();
		Index->Simplify(Context);
		auto Index = this->Index->To<IntT>();
		Replace(Base->GetIndex(Index->Value));
	}
	
	IndexT(AtomT *Base, AtomT *Index) : Base(Base), Index(Index) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	auto &LLVM = llvm::getGlobalContext();
	auto Module = new llvm::Module("asdf", LLVM);
	auto MainType = llvm::FunctionType::get(llvm::Type::getVoidTy(LLVM), false);
	auto Main = llvm::Function::Create(MainType, llvm::Function::ExternalLinkage, "main", Module);
	auto Block = llvm::BasicBlock::Create(LLVM, "entrypoint", Main);
	
	auto Memcpy = Module->getFunction("llvm.memcpy.p0i8.p0i8.i64"); // TODO - build parameter 32/64 len?
	
	SingleT Scope = new RecordT(false, nullptr, {});
	ContextT Context{LLVM, Module, Memcpy, Block, Scope};
	
	MultipleT Statements;
	/*
	// Simple assignment
	{
		SingleT Local1 = new UndefinedT;
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			new IntTypeT(ExistenceT::Dynamic),
			new IntT(nullptr, 3)
		));
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			nullptr,
			new IntT(nullptr, 7)
		));
	}*/
	/*
	// Record assignment
	{
		SingleT Local1 = new UndefinedT;
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			new RecordT
			(
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntTypeT(ExistenceT::Constant)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntTypeT(ExistenceT::Dynamic)
					)
				}
			),
			new RecordT
			(
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntT(nullptr, 25)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntT(nullptr, 39)
					)
				}
			)
		));
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			nullptr,
			new RecordT
			(
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntT(nullptr, 7)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntT(nullptr, 12)
					)
				}
			)
		));
	}
	*/
	
	/*
	// l-value record assignment
	{
		Statements.emplace_back(new AssignmentT
		(
			new ElementT
			(
				nullptr,
				new StringT(nullptr, "k")
			),
			nullptr,
			new IntT(nullptr, 77)
		));
		Statements.emplace_back(new AssignmentT
		(
			new ElementT
			(
				nullptr,
				new StringT(nullptr, "r")
			),
			new IntTypeT(ExistenceT::Dynamic),
			new ElementT
			(
				nullptr,
				new StringT(nullptr, "k")
			)
		));
		Statements.emplace_back(new AssignmentT
		(
			new RecordT
			(
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new ElementT
						(
							nullptr,
							new StringT(nullptr, "k")
						)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new ElementT
						(
							nullptr,
							new StringT(nullptr, "r")
						)
					)
				}
			),
			nullptr,
			new RecordT
			(
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntT(nullptr, 7)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntT(nullptr, 12)
					)
				}
			)
		));
	}
	*/
	
	// Struct assignment
	{
		SingleT Local1 = new UndefinedT;
		SingleT Local2 = new UndefinedT;
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			new RecordT
			(
				true,
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntTypeT(ExistenceT::Constant)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntTypeT(ExistenceT::Dynamic)
					)
				}
			),
			new RecordT
			(
				false,
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntT(nullptr, 25)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntT(nullptr, 39)
					)
				}
			)
		));
		Statements.emplace_back(new AssignmentT
		(
			Local2,
			new RecordT
			(
				true,
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntTypeT(ExistenceT::Constant)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntTypeT(ExistenceT::Dynamic)
					)
				}
			),
			new RecordT
			(
				false,
				nullptr,
				{
					new SubAssignmentT
					(
						new StringT(nullptr, "a"),
						new IntT(nullptr, 5)
					),
					new SubAssignmentT
					(
						new StringT(nullptr, "b"),
						new IntT(nullptr, 9)
					)
				}
			)
		));
		Statements.emplace_back(new AssignmentT
		(
			Local1,
			nullptr,
			Local2
		));
	}
	
	// TODO unpacking example
	
	for (auto &Statement : Statements) 
		Statement->Simplify({LLVM, Module, Block});
	
	llvm::ReturnInst::Create(LLVM, Block);
	Module->dump();
	
	return 0;
}
