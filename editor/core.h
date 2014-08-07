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

struct PathTypesT
{
	typedef std::string FieldT;
	typedef size_t IndexT;
};

struct PathElementT : VariantT<PathTypesT::FieldT, PathTypesT::IndexT>
{
	size_t Count;
	PathElementT *Parent;

	template <typename ValueT> PathElementT(ValueT const &Value, PathElementT *Parent = nullptr) : VariantT<PathTypesT::FieldT, PathTypesT::IndexT>(Value), Count(0), Parent(Parent)
	{
		if (Parent) Parent->Count += 1;	
	}
	
	~PathElementT(void);

	std::ostream &StandardStream(std::ostream &Stream);
};

struct PathT : PathTypesT
{
	using PathTypesT::FieldT;
	using PathTypesT::IndexT;

	PathT(void);
	PathT(PathT const &Other);
	PathT(PathT &&Other);
	PathT(PathElementT *Element);
	~PathT(void);
	PathT &operator =(PathT const &Other);
	PathT &operator =(PathT &&Other);
	PathT &operator =(PathElementT *Element);

	PathT Field(std::string const &Name);
	PathT Index(size_t Value);

	operator bool(void) const;
	PathElementT *operator ->(void) const;

	private:
		PathElementT *Element;
};

inline std::ostream &operator <<(std::ostream &Stream, PathT const &Path)
{
	if (Path) return Path->StandardStream(Stream);
	else return Stream << "[/]";
}

struct CoreT;

/*struct VisualT
{
	VisualT(void) = default;
	VisualT(VisualT const &) = delete;
	void Start(void);
	void Add(VisualT &Other);
	void Add(std::string const &Text);
	void Set(std::string const &Text);
	std::string Dump(void);
};*/

struct VisualT
{
	VisualT(QWebElement const &Element);
	VisualT(QWebElement const &Root, QWebElement const &Element);
	VisualT(VisualT const &) = delete;
	~VisualT(void);

	void SetClass(std::string const &Class);
	void UnsetClass(std::string const &Class);
	VisualT &Tag(void);

	void Start(void);
	void Add(VisualT &Other);
	void Add(std::string const &Text);
	void Set(std::string const &Text);
	std::string Dump(void);

	QWebElement Root;
	
	private:
		std::string const ID;
		std::string IDName(void);
	
		std::unique_ptr<VisualT> TagVisual;
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
	enum struct MainT
	{
		// Spatial keys
		Left,
		Right,
		Up,
		Down,
		TextBackspace,

		// Relative keys
		Delete,
		Enter,
		Exit,
		NewStatement
	};
	OptionalT<MainT> Main;
	OptionalT<std::string> Text;
};

enum struct FocusDirectionT
{
	FromAhead,
	FromBehind,
	Direct
};

struct AtomTypeT;

struct NucleusT
{
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
	virtual void Focus(FocusDirectionT Direction);
	virtual void Defocus(void);
	virtual void AssumeFocus(void); // If bool is false, optional must not be set
	virtual void Refresh(void);
	virtual std::unique_ptr<ActionT> Set(NucleusT *Nucleus);
	virtual std::unique_ptr<ActionT> Set(std::string const &Text); // TODO StringTypePartT/Common- should add a SetT class to the StringDataPartT class, protoatom should use that set and this should be removed
	virtual OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input);

	virtual void FocusPrevious(void);
	virtual void FocusNext(void);
		
	void FlagRefresh(void);
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
	HoldT Focused;
	
	std::unique_ptr<AtomTypeT> 
		ModuleType, 
		GroupType, 
		ElementType, 
		AssignmentType, 
		StringType;
	std::map<std::string, AtomTypeT *> Types;

	std::set<NucleusT *> DeletionCandidates;
	std::set<NucleusT *> NeedRefresh;

	VisualT CursorVisual;

	CoreT(VisualT &RootVisual);
	void HandleInput(InputT const &Input);
	OptionalT<AtomTypeT *> LookUpAtom(std::string const &Text);
	void Apply(OptionalT<std::unique_ptr<ActionT>> Action);
	std::list<std::unique_ptr<ActionT>> UndoQueue, RedoQueue;
	void Undo(void);
	void Redo(void);

	// Used by atoms (internal)
	void AssumeFocus(void);

	std::unique_ptr<ActionT> ActionHandleInput(InputT const &Input);

	std::string Dump(void) const;

	private:
		void Refresh(void);
};

struct ProtoatomT : NucleusT
{
	OptionalT<bool> IsIdentifier;
	std::string Data;
	bool Focused;
	size_t Position = 0;

	HoldT Lifted;

	ProtoatomT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &&Polymorph) const override;
	
	static AtomTypeT &StaticGetTypeInfo(void);
	AtomTypeT const &GetTypeInfo(void) const override;

	void Focus(FocusDirectionT Direction) override;
	void Defocus(void) override;

	void AssumeFocus(void) override;
	void Refresh(void) override;
	
	void Lift(NucleusT *Nucleus);
	
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	OptionalT<std::unique_ptr<ActionT>> Finish(
		OptionalT<AtomTypeT *> Type, 
		OptionalT<std::string> NewData, 
		OptionalT<InputT> SeedData);
};

}

#endif

