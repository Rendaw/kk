#ifndef core_h
#define core_h

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
		AtomT &operator =(AtomT &Other);
	
		operator NucleusT *(void);
		operator bool(void);
		
		template <typename AsT> OptionalT<AsT *> As(void) 
		{ 
			auto Out = dynamic_cast<AsT *>(Nucleus);
			if (!Out) return {};
			return {Out};
		}
		
		NucleusT *operator ->(void);
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
		AtomT Scope);
	ContextT(ContextT const &Context);
	ContextT EnterScope(AtomT Scope);
};

//================================================================================================================
// Interfaces
struct ValueT
{
	virtual ~ValueT(void);
	virtual AtomT Allocate(ContextT Context);
};

struct AssignableT
{
	virtual ~AssignableT(void);
	virtual void Assign(ContextT Context, AtomT Other);
};

// LLVM
struct LLVMLoadableT
{
	virtual ~LLVMLoadableT(void);
	virtual llvm::Value *GenerateLLVMLoad(ContextT Context) = 0;
};

struct LLVMLoadableTypeT
{
	virtual ~LLVMLoadableTypeT(void);
	virtual llvm::Type *GenerateLLVMType(ContextT Context) = 0;
};

//================================================================================================================
// Basics
struct UndefinedT : NucleusT, AssignableT
{
	void Assign(ContextT Context, AtomT Other) override;
};

struct ImplementT : NucleusT, ValueT
{
	AtomT Type, Value;
	
	AtomT Allocate(ContextT Context) override;
	void Simplify(ContextT Context) override;
};

//================================================================================================================
// Primitives
struct PrimitiveTypeT 
{
	virtual ~PrimitiveTypeT(void);
	virtual AtomT Allocate(ContextT Context);
	virtual void Assign(ContextT Context, llvm::Value *Target, AtomT Other);
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
	
	void Assign(ContextT Context, AtomT Other) override;
};

struct StringTypeT : NucleusT, PrimitiveTypeT
{
	uint16_t ID;
	// No dynamic strings, for now at least
	bool Static;

	void CheckType(bool Defined, AtomT Other);
	AtomT Allocate(ContextT Context) override;
	void Assign(ContextT Context, std::string &Data, AtomT Other);
	void Assign(ContextT Context, llvm::Value *Target, AtomT Other) override;
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
	
	void Assign(ContextT Context, AtomT Other) override;
	llvm::Value *GenerateLLVMLoad(ContextT Context) override;
};

struct DynamicT : NucleusT, AssignableT, LLVMLoadableT
{
	bool Defined;
	AtomT Type;
	llvm::Value *Target;
	
	AtomT Allocate(ContextT Context) override;
	void Assign(ContextT Context, AtomT Other) override;
	llvm::Value *GenerateLLVMLoad(ContextT Context) override;
};

enum struct LLVMConversionTypeT { Signed, Unsigned, Float };
llvm::Value *GenerateLLVMNumericConversion(llvm::BasicBlock *Block, llvm::Value *Source, llvm::Type *SourceType, bool SourceSigned, llvm::Type *DestType, bool DestSigned);

struct NumericTypeT : NucleusT, PrimitiveTypeT, LLVMLoadableTypeT
{
	uint16_t ID;
	bool Constant;
	bool Static;
	enum struct DataTypeT { Int, UInt, Float, Double } DataType;
	
	bool IsSigned(void) const;
	void CheckType(bool Defined, AtomT Other);
	AtomT Allocate(ContextT Context) override;
	void Assign(ContextT Context, bool &Defined, llvm::Value *Target, AtomT Other) override;
	llvm::Type *GenerateLLVMType(ContextT Context) override;
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

struct GroupT : NucleusT, ValueT, AssignableT, GroupCollectionT
{
	std::vector<AtomT> Statements;

	void Simplify(ContextT Context) override;
	AtomT Allocate(ContextT Context) override;
	void Assign(ContextT Context, AtomT Other) override;
	AtomT AccessElement(AtomT Key);
};

struct ElementT : NucleusT
{
	AtomT Base, Key;

	void Simplify(ContextT Context) override;
};

//================================================================================================================
// Statements
struct AssignmentT : NucleusT
{
	AtomT Left, Right;

	void Simplify(ContextT Context) override;
};

//================================================================================================================
// Functions

}

#endif

