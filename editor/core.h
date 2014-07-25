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
	VisualT(void) = default;
	VisualT(VisualT const &) = delete;
	void Clear(void);
	void Add(VisualT &Other);
	void Add(std::string const &Text);
	void Set(std::string const &Text);
	std::string Dump(void);
};

/*struct VisualT
{
	VisualT(QWebElement const &Element);
	VisualT(QWebElement const &Root, QWebElement const &Element);
	VisualT(VisualT const &) = delete;
	~VisualT(void);
	void Clear(void);
	void Add(VisualT &Other);
	void Add(std::string const &Text);
	void Set(std::string const &Text);
	std::string Dump(void);

	QWebElement Root;
	
	private:
		std::string const ID;
		std::string IDName(void);
};*/

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
	AtomT(AtomT &&Other);
	AtomT &operator =(AtomT &&Other);
	AtomT(AtomT const &Other) = delete;
	AtomT(CoreT &Core);
	~AtomT(void);
	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	operator bool(void) const;

	void ImmediateSet(NucleusT *Nucleus);
	std::unique_ptr<ActionT> Set(NucleusT *Nucleus);
	
	typedef std::function<void(AtomT &)> AtomCallbackT;
	CoreT &Core;
	AtomCallbackT Callback;
	NucleusT *Parent;
	NucleusT *Nucleus;

	private:
		void Clear(void);
};

struct HoldT
{
	HoldT(CoreT &Core);
	HoldT(CoreT &Core, NucleusT *Nucleus);
	HoldT(HoldT const &Other);
	~HoldT(void);
	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	HoldT &operator =(NucleusT *Nucleus);
	void Set(NucleusT *Nucleus);
	void Clear(void);
	
	operator bool(void) const;
	
	CoreT &Core;
	NucleusT *Nucleus;
};

enum ArityT
{
	Nullary,
	Unary,
	Binary
};

struct InputT
{
	std::string Text;
};

struct AtomTypeT;

struct NucleusT
{
	public:
		CoreT &Core;
		HoldT Parent;
		VisualT Visual;

		size_t Count = 0;
		AtomT *Atom;
		
		NucleusT(CoreT &Core);
		virtual ~NucleusT(void);

		template <typename AsT> OptionalT<AsT *> As(void) 
		{ 
			auto Out = dynamic_cast<AsT *>(this); 
			if (Out) return Out; 
			return {}; 
		}

		void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const;
		virtual void Serialize(Serial::WritePolymorphT &&Polymorph) const;

		virtual AtomTypeT const &GetTypeInfo(void) const;
		virtual void Refocus(void); // If bool is false, optional must not be set
		virtual void Refresh(void);
		virtual std::unique_ptr<ActionT> Place(NucleusT *Nucleus);
		virtual OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input);
};

struct AtomTypeT
{
	AtomTypeT(void) = default;
	AtomTypeT(AtomTypeT const &) = delete;
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
	HoldT Focus;
	
	std::map<std::string, AtomTypeT *> Types;

	std::set<NucleusT *> DeletionCandidates;
	std::set<NucleusT *> NeedRefresh;

	CoreT(VisualT &RootVisual);
	void HandleInput(InputT const &Input);
	OptionalT<AtomTypeT *> LookUpAtom(std::string const &Text);
	void Apply(OptionalT<std::unique_ptr<ActionT>> Action);
	std::list<std::unique_ptr<ActionT>> UndoQueue, RedoQueue;
	void Undo(void);
	void Redo(void);

	// Used by atoms (internal)
	bool IsIdentifierClass(std::string const &Reference);
	void Refocus(void);

	std::unique_ptr<ActionT> ActionHandleInput(InputT const &Input);

	std::string Dump(void) const;
};

struct ModuleT : NucleusT
{
	AtomT Top;

	ModuleT(CoreT &Core);
		
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refocus(void) override;
	void Refresh(void) override;
};

struct GroupT : NucleusT
{
	AtomT::AtomCallbackT AtomCallback;
	std::vector<AtomT> Statements;
	size_t Focus;
	
	GroupT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refocus(void) override;
	void Refresh(void) override;

	void AddStatement(size_t Index);
	void RemoveStatement(size_t Index);
	std::unique_ptr<ActionT> ModifyStatements(bool Add, size_t Index);
	std::unique_ptr<ActionT> Set(size_t Index, NucleusT *Value);
};

struct ProtoatomT : NucleusT
{
	OptionalT<bool> IsIdentifier;
	std::string Data;
	size_t Position = 0;

	HoldT Lifted;

	ProtoatomT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refocus(void) override;
	void Refresh(void) override;
	
	void Lift(NucleusT *Nucleus);
	
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	OptionalT<std::unique_ptr<ActionT>> Finish(
		OptionalT<AtomTypeT *> Type, 
		OptionalT<std::string> NewData, 
		OptionalT<InputT> SeedData);
};

struct ElementT : NucleusT
{
	AtomT Base, Key;
	bool BaseFocused;

	ElementT(CoreT &Core);

	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refocus(void) override;
	void Refresh(void) override;

	std::unique_ptr<ActionT> Place(NucleusT *Nucleus) override;
	std::unique_ptr<ActionT> PlaceKey(NucleusT *Nucleus);
};

struct StringT : NucleusT
{
	std::string Data;
	
	StringT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refresh(void) override;

	std::unique_ptr<ActionT> Set(std::string const &Text);
};

struct AssignmentT : NucleusT
{
	AtomT Left, Right;
	bool LeftFocused;

	AssignmentT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;
	void Refocus(void) override;
	void Refresh(void) override;
	
	std::unique_ptr<ActionT> Place(NucleusT *Nucleus) override;
};

struct IntT : NucleusT
{
};

}

#endif

