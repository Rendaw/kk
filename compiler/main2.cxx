#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Type.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/Instructions.h>
#include <llvm/CallingConv.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/ModuleProvider.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

struct AtomT
{
	virtual ~AtomT(void) {}
};

typedef std::unique_ptr<AtomT> SingleT;
typedef std::list<std::unique_ptr<AtomT>> MultipleT;

// Simplify error checks and reduces if constant
// All other functions may not fail

// External
struct ModuleT : AtomT
{
	MultipleT Statements;
	
	void Simplify(void)
	{
		auto &Context = llvm::getGlobalContext(); 
		auto Module = new llvm::Module("kk-1", Context); // Name from filename, probably would be best
		
		for (auto &Statement : Statements) 
		{
			Statement->Simplify(this);
		}
		Module->dump();
	}
};

struct ExportT : AtomT
{
	SingleT Statement;
	
	SingleT Scope;
	
	void Simplify(llvm::Module *Module)
	{
		if (!Statement) return Error(Position, "Export must contain a single assignment.");
		Statement->Simplify(Module);
		auto Assignment = Statement.To<AssignmentT>();
		if (!Assignment) return Error(Position, String() << "Export must contain a single assignment.  Got a " << Assignment->TypeName());
		if (auto Function = Assignment->Value->To<FunctionT>())
		{
			if (auto DynamicFunction = Function->Refine({}))
			{
				auto Type = DynamicFunction->GetLLVMType();
				auto CallTarget = DynamicFunction->GetLLVM();
				auto Out = llvm::Function:::Create(Type, llvm::Function::ExternalLinkage, name, Module);
				BasicBlock* OutBlock = llvm::BasicBlock::Create(getGlobalContext(), "entry", Out);
				auto Return = llvm::CallInst::Create(Call, Out->getArgumentList(), "", OutBlock);
				llvm::ReturnInst::Create(llvm::getGlobalContext(), Return, OutBlock);
			}
		}
	}
};

struct ElementT : AtomT
{
	SingleT Base;
	SingleT Name;
	
	void Simplify(void)
	{
		Base->Simplify();
		Name->Simplify();
		auto NameString = Name->To<StringT>();
		if (!NameString) return Error(Position, String() << "Element name must be a string or simplifiable to a string, but it is an irreducible " << Name->TypeString());
		auto BaseRecord = Base->To<RecordT>();
	}
	
	void LLVMGet(void)
	{
		
	}
};

struct IndexT : AtomT
{
	SingleT Base;
	SingleT Index;
	
	void Simplify(void)
	{
		
	}
	
	void LLVMGet(void)
	{
		
	}
	
	void LLVMSet(void)
	{
	}
};

struct AssignmentT : AtomT
{
	SingleT Target;
	SingleT Value;
	
	SingleT Context;
	SingleT Parent;
	
	void Simplify(void)
	{
		Target->Simplify();
		Value->Simplify();
		
		if (Value->IsConstant())
		{
			if (auto Name = Target->To<NameT>())
			{
			}
			else
			{
				
			}
			Replace(nullptr);
		}
		
		if (auto Element = Target->To<ElementT>())
		{
			// Element is either dynamic or undefined
			if (Element->IsUndefined())
			{
				if (Value->IsConstant())
				{
					Element->Base->Set(Element->Name, Value);
					Replace(nullptr);
				}
				else
				{
					// LLVM, generate alloca
				}
			}
		}
		else if (auto Index = Target->To<IndexT>())
		{
			
		}
		else if (Target->IsConstant())
		{
			if (!Value->IsConstant()) return Error(Position, "Cannot assign dynamic value to constant element.");
			Target->Replace(Value);
		}
		else {} // Dynamic assignment
		
		if (auto Record = Parent->To<RecordT>(
	}
	
	void LLVM(void)
	{
		// Value already simplified
		// Not constant -> couldn't simplify down to nothing
		if (auto Function = Value->To<FunctionT>())
		{
			auto DynamicFunction = Function->Refine({});
			if (!DynamicFunction) return Error(Position, "Function must have only run-time arguments.");
			
			auto Type = DynamicFunction->LLVMType();
			auto Call = DynamicFunction->LLVM();
			auto Out = llvm::Function:::Create(Type, llvm::Function::ExternalLinkage, name, Module);
			BasicBlock* OutBlock = llvm::BasicBlock::Create(getGlobalContext(), "entry", Out);
			auto Return = llvm::CallInst::Create(Call, Out->getArgumentList(), "", OutBlock);
			llvm::ReturnInst::Create(llvm::getGlobalContext(), Return, OutBlock);
		}
	}
};

// Internal

int main(int argc, char **argv)
{
	return 0;
}
