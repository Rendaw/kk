#ifndef core_h
#define core_h

/*NOTE NOTE NOTE NOTe
] branch initialized
] context branchallocatestate = none, toplevel, parent = allocates restricted
] stack based shared context

] make atoms always valid, use optional
 
] macros
] export
] import, c ffi?

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
#include <llvm/PassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

//#define ERROR assert(false)
//#define ERROR { std::cout << "Error at " << Position->AsString() << std::endl; assert(false); } while (0)
#define ERROR { std::cout << "Error at " << (Context.Position ? Context.Position->AsString() : "NO POSITION") << std::endl; assert(false); } while (0)

namespace Core
{

struct PositionBaseT
{
	virtual ~PositionBaseT(void);
	virtual std::string AsString(void) const = 0;
};

typedef std::shared_ptr<PositionBaseT> PositionT;

#define HARDPOSITION std::make_shared<HardPositionT>(__FILE__, __FUNCTION__, __LINE__)
#define INVALIDPOSITION std::make_shared<HardPositionT>("invalid", "", 0)
struct HardPositionT : PositionBaseT
{
	std::string const Position;
	HardPositionT(char const *File, char const *Function, int Line);
	std::string AsString(void) const override;
};

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
		PositionT const Position;
		NucleusT(PositionT const Position);
		void Replace(NucleusT *Replacement);
	public:
		virtual ~NucleusT(void);
		virtual AtomT Clone(void);
		virtual AtomT GetType(ContextT &Context);
		virtual void Simplify(ContextT &Context);
};

struct AtomT
{
	private:
		NucleusT *Nucleus;
	
	public:
		void Set(NucleusT *Nucleus);
		void Clear(void);
		AtomT(void);
		AtomT(NucleusT *Nucleus);
		AtomT(AtomT const &Other);
		~AtomT(void);
		
		AtomT &operator =(NucleusT *Nucleus);
		AtomT &operator =(AtomT const &Other);
	
		operator NucleusT *(void);
		operator bool(void) const;
		bool operator !(void) const;
		
		template <typename AsT> OptionalT<AsT *> As(void) 
		{ 
			auto Out = dynamic_cast<AsT *>(Nucleus);
			if (!Out) return {};
			return {Out};
		}
		
		template <typename AsT> OptionalT<AsT const *> As(void) const
		{ 
			auto Out = dynamic_cast<AsT const *>(Nucleus);
			if (!Out) return {};
			return {Out};
		}
		
		NucleusT *operator ->(void);
};

struct ContextT
{
	llvm::LLVMContext &LLVM;
	llvm::Module *Module;
	
	llvm::BasicBlock *EntryBlock;
	std::shared_ptr<llvm::BasicBlock *> Block;
	AtomT Scope;
	
	PositionT Position;
	
	bool IsConstant;
	
	std::shared_ptr<uint16_t> TypeIDCounter;
	
	ContextT(llvm::LLVMContext &LLVM);
};

struct BranchableBoolT
{
	public:
		struct ValueT
		{
			ValueT(PositionT Position, bool Value);
			ValueT(ValueT const &Other);
			PositionT Position;
			bool Value;
		};
		
		BranchableBoolT(PositionT Position = {}, OptionalT<ValueT> Base = {});
		BranchableBoolT &operator =(BranchableBoolT const &Other);
		bool Get(void);
		void Set(ContextT &Context);
		void Unset(ContextT &Context);
		void EnterBranch(ContextT &Context);
		void ExitBranch(ContextT &Context);
		void Finish(void);
		
	private:
		PositionT Position;
		OptionalT<ValueT> Base, Model;
		OptionalT<std::shared_ptr<BranchableBoolT>> Branch;
};

//================================================================================================================
// Interfaces
struct TypeT
{
	virtual ~TypeT(void);
	virtual AtomT Allocate(ContextT &Context, AtomT Value) = 0;
	virtual bool IsVariable(void) = 0;
	virtual void CheckType(ContextT &Context, AtomT Other) = 0;
};

struct ValueT
{
	BranchableBoolT Initialized;
	
	virtual ~ValueT(void);
	virtual void Assign(ContextT &Context, AtomT Other) = 0;
};

// LLVM
struct LLVMLoadableT
{
	virtual ~LLVMLoadableT(void);
	virtual llvm::Value *GenerateLLVMLoad(ContextT &Context) = 0;
};

struct LLVMLoadableTypeT
{
	virtual ~LLVMLoadableTypeT(void);
	virtual llvm::Type *GenerateLLVMType(ContextT &Context) = 0;
};

struct LLVMAssignableTypeT
{
	virtual ~LLVMAssignableTypeT(void);
	virtual void AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other) = 0;
};

//================================================================================================================
// Basics
struct UndefinedT : NucleusT, ValueT
{
	UndefinedT(PositionT const Position);
	void Assign(ContextT &Context, AtomT Other) override;
};

struct ImplementT : NucleusT
{
	AtomT Type, Value;
	
	ImplementT(PositionT const Position);
	AtomT Clone(void) override;
	AtomT GetType(ContextT &Context) override;
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Primitives

struct StringTypeT;
struct StringT : virtual NucleusT, virtual ValueT
{
	AtomT Type;
	std::string Data;
	
	StringT(PositionT const Position);
	AtomT Clone(void) override;
	AtomT GetType(ContextT &Context) override;
	void Assign(ContextT &Context, AtomT Other) override;
};

struct StringTypeT : NucleusT, TypeT
{
	uint16_t ID;
	// No variable strings, for now at least
	bool Static;
	
	StringTypeT(PositionT const Position);
	AtomT Clone(void) override;
	bool IsVariable(void) override;
	void CheckType(ContextT &Context, AtomT Other) override;
	AtomT Allocate(ContextT &Context, AtomT Value) override;
	void Assign(ContextT &Context, BranchableBoolT &Initialized, std::string &Data, AtomT Other);
};

template <typename DataT> struct NumericT : 
	virtual NucleusT, 
	virtual ValueT, 
	virtual LLVMLoadableT
{
	AtomT Type;
	DataT Data;
	
	NumericT(PositionT const Position);
	AtomT Clone(void) override;
	AtomT GetType(ContextT &Context) override;
	void Assign(ContextT &Context, AtomT Other) override;
	llvm::Value *GenerateLLVMLoad(ContextT &Context) override;
};

struct VariableT : NucleusT, ValueT, LLVMLoadableT
{
	AtomT Type;
	struct StructTargetT
	{
		llvm::Value *Struct;
		size_t Index;
	};
	OptionalT<StructTargetT> StructTarget;
	llvm::Value *Target;
	
	VariableT(PositionT const Position);
	
	AtomT GetType(ContextT &Context) override;
	llvm::Value *GetTarget(ContextT &Context);
	void Assign(ContextT &Context, AtomT Other) override;
	llvm::Value *GenerateLLVMLoad(ContextT &Context) override;
};

typedef uint64_t TypeIDT;
struct NumericTypeT : NucleusT, TypeT, LLVMLoadableTypeT, LLVMAssignableTypeT
{
	uint16_t ID;
	bool Constant;
	bool Static;
	enum struct DataTypeT { Int, UInt, Float, Double } DataType;
	
	NumericTypeT(PositionT const Position);
	AtomT Clone(void) override;
	bool IsVariable(void) override;
	bool IsSigned(void) const;
	void CheckType(ContextT &Context, AtomT Other) override;
	AtomT Allocate(ContextT &Context, AtomT Value) override;
	void AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other) override;
	llvm::Type *GenerateLLVMType(ContextT &Context) override;
};

//================================================================================================================
// Groups
struct GroupCollectionT
{
	std::map<std::string, AtomT> Keys;

	OptionalT<AtomT> GetByKey(std::string const &Key);
	void Add(std::string const &Key, AtomT Value);
	auto begin(void) -> decltype(Keys.begin());
	auto end(void) -> decltype(Keys.end());
};

struct GroupT : NucleusT, ValueT, GroupCollectionT
{
	OptionalT<AtomT> Parent; // Compiler use only
	bool IsFunctionTop; // Compiler use only
	// TODO? gettype, allocate
	std::vector<AtomT> Statements;

	GroupT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
	void Assign(ContextT &Context, AtomT Other) override;
	AtomT AccessElement(ContextT &Context, std::string const &Key);
	AtomT AccessElement(ContextT &Context, AtomT Key);
};

struct BlockT : NucleusT
{
	std::vector<AtomT> Statements;
	
	BlockT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
	AtomT CloneGroup(void);
};

struct ElementT : NucleusT
{
	AtomT Base, Key;

	ElementT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Type manipulations
struct AsVariableTypeT : NucleusT
{
	AtomT Type;
	
	AsVariableTypeT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Statements
struct AssignmentT : NucleusT
{
	AtomT Left, Right;

	AssignmentT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Functions
template <typename TypeT> struct FunctionTreeT
{
	struct ElementT
	{
		OptionalT<TypeT> Leaf;
		std::map<std::vector<uint8_t>, std::unique_ptr<ElementT>> Branches;
	} Root;
	
	struct LookupT
	{
		ElementT *Position;
		void Enter(std::vector<uint8_t> const &Key)
		{
			auto &Branch = Position->Branches[Key];
			if (!Branch) Branch.reset(new ElementT);
		}
		
		operator bool(void) const { return Position->Leaf; }
		
		TypeT &operator *(void) { if (!Position->Leaf) Position->Leaf = TypeT{}; return *Position->Leaf; }
		
		TypeT *operator ->(void) { if (!Position->Leaf) Position->Leaf = TypeT{}; return &*Position->Leaf; }
	};
	
	LookupT StartLookup(void) { return LookupT{&Root}; }
};

struct FunctionT : NucleusT
{
	AtomT Type, Body;
	
	AtomT ParentScope; // Compiler use only
	
	struct CachedLLVMFunctionT
	{
		llvm::Value *Function;
		bool IsConstant;
	};
	FunctionTreeT<CachedLLVMFunctionT> InstanceTree;
	
	FunctionT(PositionT const Position);
	AtomT GetType(ContextT &Context) override;
	void Simplify(ContextT &Context) override;
	
	AtomT Call(ContextT &Context, AtomT Input);
};

struct FunctionTypeT : NucleusT, TypeT, LLVMLoadableTypeT, LLVMAssignableTypeT
{
	uint16_t ID;
	bool Constant;
	bool Static;
	AtomT Signature;
	
	struct CachedLLVMFunctionTypeT
	{
		llvm::FunctionType *FunctionType;
		OptionalT<llvm::Type *> ResultStructType;
	};
	FunctionTreeT<CachedLLVMFunctionTypeT> TypeTree;
	
	FunctionTypeT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
	AtomT Allocate(ContextT &Context, AtomT Value) override;
	bool IsVariable(void) override;
	void CheckType(ContextT &Context, AtomT Other) override;
	llvm::Type *GenerateLLVMType(ContextT &Context) override;
	void AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other) override;
	
	AtomT Call(ContextT &Context, AtomT Body, AtomT Input);
	
	struct GenerateLLVMTypeParamsT {};
	struct GenerateLLVMTypeResultsT
	{
		llvm::Type *Type;
	};
	struct GenerateLLVMLoadParamsT
	{
		FunctionT *Function;
	};
	struct GenerateLLVMLoadResultsT
	{
		llvm::Value *Value;
	};
	struct CallParamsT
	{
		AtomT Function;
		AtomT Input;
	};
	struct CallResultsT
	{
		AtomT Result;
	};
	typedef VariantT<GenerateLLVMTypeParamsT, GenerateLLVMLoadParamsT, CallParamsT> ProcessFunctionParamT;
	typedef VariantT<GenerateLLVMTypeResultsT, GenerateLLVMLoadResultsT, CallResultsT> ProcessFunctionResultT;
	
	ProcessFunctionResultT ProcessFunction(ContextT &Context, ProcessFunctionParamT Param);
};

struct CallT : NucleusT
{
	AtomT Function, Input;
	
	CallT(PositionT const Position);
	AtomT Clone(void) override;
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Module stuff
struct ModuleT : NucleusT
{
	std::string Name;
	bool Entry;
	AtomT Top;
	
	ModuleT(PositionT const Position);
	void Simplify(ContextT &Context) override;
};

//================================================================================================================
// Variants
struct VariantValueT : NucleusT, ValueT // Internal
{
	AtomT Variant;
	AtomT Class;
	AtomT Data;
	
	VariantValueT(PositionT const Position);
	void Assign(ContextT &Context, AtomT Other) override;
};

struct VariantClassT : NucleusT, TypeT // Internal
{
	AtomT Variant;
	AtomT BaseType;
	size_t Index;
	bool Constant;
	bool Static;
	
	VariantClassT(PositionT const Position);
	AtomT Allocate(ContextT &Context, AtomT Value) override;
	bool IsVariable(void) override;
	void CheckType(ContextT &Context, AtomT Other) override;
	
	llvm::Value *GenerateLLVMLoadID(ContextT &Context);
};

struct VariantTypeT : GroupT, TypeT, LLVMLoadableTypeT, LLVMAssignableTypeT // Internal
{
	uint16_t ID;
	bool Constant;
	bool Static;
	
	llvm::Type *LLVMIndexType;
	llvm::Type *LLVMType;
	
	VariantTypeT(PositionT const Position);
	AtomT Allocate(ContextT &Context, AtomT Value) override;
	AtomT Allocate(ContextT &Context, AtomT Value, bool Constant, bool Static, AtomT Class);
	bool IsVariable(void) override;
	void CheckType(ContextT &Context, AtomT Other) override;
	llvm::Type *GenerateLLVMType(ContextT &Context) override;
	using GroupT::Assign;
	void Assign(ContextT &Context, BranchableBoolT Initialized, AtomT &Data, AtomT Other);
	void AssignLLVM(ContextT &Context, BranchableBoolT &Initialized, llvm::Value *Target, AtomT Other) override;
	
	llvm::Type *GenerateLLVMIndexType(ContextT &Context);
};

struct VariantT : NucleusT
{
	AtomT Types;
	
	VariantT(PositionT const Position);
	AtomT Clone(void);
	void Simplify(ContextT &Context);
};

struct IdentifyT : NucleusT
{
	AtomT Subject;
	AtomT Criteria;
	AtomT Body;
	AtomT Else;
	
	IdentifyT(PositionT const Position);
	AtomT Clone(void);
	void Simplify(ContextT &Context);
};

}

#endif

