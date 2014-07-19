#ifndef core_h
#define core_h

#include <functional>
#include <list>
#include <set>
#include <vector>
#include <memory>
#include <QWebElement>

#include "../shared/type.h"
#include "../shared/serial.h"

namespace Core
{

struct CoreT;
struct VisualT
{
	VisualT(QWebElement const &Element);
	VisualT(QWebElement const &Root, QWebElement const &Element);
	VisualT(VisualT const &) = delete;
	~VisualT(void);
	void Clear(void);
	void Add(VisualT &Other);
	void Set(std::string const &Text);
	std::string Dump(void);

	QWebElement Root;
	
	private:
		std::string const ID;
		std::string IDName(void);
};

struct ActionT
{
	virtual ~ActionT(void);
	virtual std::unique_ptr<ActionT> Apply(void) = 0;
	virtual bool Combine(std::unique_ptr<ActionT> &Other);
};

struct ActionGroupT : ActionT
{
	void Add(std::unique_ptr<ActionT> Action);
	void AddReverse(std::unique_ptr<ActionT> Action);
	std::unique_ptr<ActionT> Apply(void);
	std::list<std::unique_ptr<ActionT>> Actions;
};

struct NucleusT;

struct AtomT
{
	AtomT(void);
	//AtomT(NucleusT *Nucleus);
	AtomT(AtomT const &Other);
	~AtomT(void);
	NucleusT *operator ->(void);
	AtomT &operator =(NucleusT *Nucleus);
	void Set(NucleusT *Nucleus);
	void Clear(void);
	
	operator bool(void);
	
	typedef std::function<void(AtomT &)> AtomCallbackT;
	AtomCallbackT Callback;
	NucleusT *Nucleus;
};

struct HoldT
{
	HoldT(void);
	HoldT(NucleusT *Nucleus);
	HoldT(HoldT const &Other);
	~HoldT(void);
	NucleusT *operator ->(void);
	HoldT &operator =(NucleusT *Nucleus);
	void Set(NucleusT *Nucleus);
	void Clear(void);
	
	operator bool(void);
	
	NucleusT *Nucleus;
};

enum ArityT
{
	Nullary,
	Unary,
	Binary
};

struct AtomTypeT;

struct NucleusT
{
	public:
		CoreT &Core;
		AtomT Parent;
		VisualT Visual;

		size_t Count = 0;
		std::set<AtomT *> Atoms;
		
		NucleusT(CoreT &Core);
		virtual ~NucleusT(void);

		template <typename AsT> OptionalT<AsT *> As(void) 
		{ 
			auto Out = dynamic_cast<AsT *>(this); 
			if (Out) return Out; 
			return {}; 
		}

		void Serialize(Serial::WritePrepolymorphT &&Prepolymorph);
		virtual void Serialize(Serial::WritePolymorphT &&Polymorph);

		std::unique_ptr<ActionT> Replace(HoldT Other);
		std::unique_ptr<ActionT> Wedge(OptionalT<HoldT> NewBase);

		virtual AtomTypeT &GetTypeInfo(void);
		virtual void Place(NucleusT *Nucleus);
		virtual OptionalT<std::unique_ptr<ActionT>> HandleKey(std::string const &Test);
	private:
		void ImmediateReplace(NucleusT *Replacement);
};

struct AtomTypeT
{
	virtual ~AtomTypeT(void);
	virtual NucleusT *Generate(CoreT &Core);

	std::string Tag = "EditorUnimplemented";

	bool ReplaceImmediately = false;
	ArityT Arity = ArityT::Nullary;
	bool Prefix = true;
	int Precedence = 1000;
	bool LeftAssociative = true;
};

struct CoreT
{
	VisualT &RootVisual;
	AtomT Root;
	AtomT Focus;
	
	std::map<std::string, AtomTypeT *> Types;

	CoreT(VisualT &RootVisual);
	void HandleKey(std::string const &Text);
	OptionalT<AtomTypeT> LookUpAtom(std::string const &Text);
	void Apply(OptionalT<std::unique_ptr<ActionT>> Action);
	std::list<std::unique_ptr<ActionT>> UndoQueue, RedoQueue;
	void Undo(void);
	void Redo(void);

	bool IsIdentifierClass(std::string const &Reference);
};

struct ModuleT : NucleusT
{
	AtomT Top;

	ModuleT(CoreT &Core);
		
	void Serialize(Serial::WritePolymorphT &&Polymorph) override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;
};

struct GroupT : NucleusT
{
	AtomT::AtomCallbackT AtomCallback;
	std::vector<AtomT> Statements;
	
	GroupT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;
};

struct ProtoatomT : NucleusT
{
	OptionalT<bool> IsIdentifier;
	std::string Data;
	size_t Position = 0;

	HoldT Lifted;

	ProtoatomT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;
	
	OptionalT<std::unique_ptr<ActionT>> HandleKey(std::string const &Text) override;
	OptionalT<std::unique_ptr<ActionT>> Finish(
		OptionalT<AtomTypeT> Type, 
		OptionalT<std::string> NewData, 
		OptionalT<std::string> SeedData);
	void Refresh(void);
};

struct ElementT : NucleusT
{
	using NucleusT::NucleusT;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;

	void PlaceKey(NucleusT *Nucleus);
};

struct StringT : NucleusT
{
	std::string Data;
	
	using NucleusT::NucleusT;
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;
};

struct AssignmentT : NucleusT
{
	using NucleusT::NucleusT;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT &GetTypeInfo(void) override;
};

struct IntT : NucleusT
{
};

}

#endif

