#ifndef core_h
#define core_h

#include <functional>
#include <list>
#include <set>
#include <vector>
#include <memory>
#include <regex>
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

struct ReactionT
{
	virtual ~ReactionT(void);
	virtual std::unique_ptr<ReactionT> Apply(void) = 0;
	virtual bool Combine(std::unique_ptr<ReactionT> &Other);
};

struct ActionT
{
	struct ArgumentT
	{
		virtual ~ArgumentT(void);
	};
	struct TextArgumentT : ArgumentT
	{
		std::string Data;
		OptionalT<std::regex> Regex;
	};

	std::string Name;
	std::vector<ArgumentT *> Arguments;

	ActionT(std::string const &Name);
	virtual ~ActionT(void);
	virtual OptionalT<std::unique_ptr<ReactionT>> Apply(void) = 0;
};

struct ReactionGroupT : ReactionT
{
	void Add(std::unique_ptr<ReactionT> Reaction);
	void AddReverse(std::unique_ptr<ReactionT> Reaction);
	std::unique_ptr<ReactionT> Apply(void);
	std::list<std::unique_ptr<ReactionT>> Reactions;
};

struct NucleusT;

struct HoldT
{
	HoldT(CoreT &Core);
	HoldT(CoreT &Core, NucleusT *Nucleus);
	HoldT(HoldT const &Other);
	~HoldT(void);
	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	HoldT &operator =(NucleusT *Nucleus);
	HoldT &operator =(HoldT &&Hold);
	void Set(NucleusT *Nucleus);
	void Clear(void);
	
	operator bool(void) const;
	
	CoreT &Core;
	NucleusT *Nucleus;
};

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

	struct SetT : ReactionT
	{
		AtomT &Atom;
		HoldT Replacement;

		SetT(AtomT &Atom, NucleusT *Replacement);
		
		std::unique_ptr<ReactionT> Apply(void);
	};
	void Set(NucleusT *Nucleus);
	
	typedef std::function<void(NucleusT *Nucleus)> AtomCallbackT;
	CoreT &Core;
	AtomCallbackT Callback;
	NucleusT *Parent;
	NucleusT *Nucleus;

	private:
		void Clear(void);
};

enum ArityT
{
	Nullary,
	Unary,
	Binary
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
	OptionalT<NucleusT *> PartParent(void);
	VisualT Visual;

	size_t Count = 0;
	AtomT *Atom;

	std::map<uintptr_t, std::function<void(NucleusT *Changed)>> StatusWatchers;
	
	NucleusT(CoreT &Core);
	virtual ~NucleusT(void);

	template <typename AsT> OptionalT<AsT *> As(void) 
	{ 
		auto Out = dynamic_cast<AsT *>(this); 
		if (Out) return Out; 
		return {}; 
	}

	template <typename AsT> OptionalT<AsT const *> As(void) const
	{ 
		auto Out = dynamic_cast<AsT const *>(this); 
		if (Out) return Out; 
		return {}; 
	}
	
	virtual Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) = 0;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const;
	virtual void Serialize(Serial::WritePolymorphT &Polymorph) const;
	virtual AtomTypeT const &GetTypeInfo(void) const;
	virtual void Focus(FocusDirectionT Direction);
	virtual void RegisterActions(void) = 0;
	virtual void Defocus(void);
	virtual void AssumeFocus(void); // If bool is false, optional must not be set
	virtual void Refresh(void);
	virtual std::unique_ptr<ReactionT> Set(NucleusT *Nucleus);
	virtual std::unique_ptr<ReactionT> Set(std::string const &Text); // TODO StringTypePartT/Common- should add a SetT class to the StringDataPartT class, protoatom should use that set and this should be removed

	virtual void FocusPrevious(void);
	virtual void FocusNext(void);
	
	virtual bool IsEmpty(void) const;
	virtual bool IsFocused(void) const;
		
	void FlagRefresh(void);

	void FlagStatusChange(void);
	void WatchStatus(uintptr_t ID, std::function<void(NucleusT *Changed)> Callback);
	void IgnoreStatus(uintptr_t ID);
};

struct AtomTypeT
{
	AtomTypeT(void) = default;
	AtomTypeT(AtomTypeT const &) = delete;
	virtual ~AtomTypeT(void);
	virtual Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object);
	virtual void Serialize(Serial::WriteObjectT &Object) const;
	virtual NucleusT *Generate(CoreT &Core);

	std::string Tag = "EditorUnimplemented";
	OptionalT<std::string> LookupSequence;

	bool ReplaceImmediately = false;
	ArityT Arity = ArityT::Nullary;
	bool Prefix = true;
	int Precedence = 1000;
	bool LeftAssociative = true;
	
	bool SpatiallyVertical = false;
};

struct FocusT : ReactionT
{
	CoreT &Core;
	HoldT Target;
	bool DoNothing;
	FocusT(CoreT &Core, NucleusT *Nucleus, bool DoNothing = false);
	std::unique_ptr<ReactionT> Apply(void);
};

struct CoreT
{
	VisualT &RootVisual;
	AtomT Root;
	HoldT Focused;
	
	bool TextMode;
	
	std::map<std::string, std::unique_ptr<AtomTypeT>> Types;
	AtomTypeT *ProtoatomType, *ElementType, *StringType;
	std::map<std::string, AtomTypeT *> TypeLookup;

	std::set<NucleusT *> DeletionCandidates;
	std::set<NucleusT *> NeedRefresh;

	VisualT CursorVisual;

	std::list<std::shared_ptr<ActionT>> Actions;
	std::function<void(void)> ResetActionsCallback;
	std::function<void(std::shared_ptr<ActionT> Action)> RegisterActionCallback;

	CoreT(VisualT &RootVisual);
	~CoreT(void);
	void Serialize(Filesystem::PathT const &Path);
	void Deserialize(Filesystem::PathT const &Path);
	Serial::ReadErrorT Deserialize(AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object);
	void HandleInput(std::shared_ptr<ActionT> Action);
	//void HandleTextInput(std::string const &ActionName, std::string const &Text); // Unimplemented
	OptionalT<AtomTypeT *> LookUpAtom(std::string const &Text);
	
	void Apply(OptionalT<std::unique_ptr<ReactionT>> Reaction);
	std::list<std::unique_ptr<ReactionT>> UndoQueue, RedoQueue;
	bool HasChanges(void);
	void Undo(void);
	void Redo(void);

	// Used by atoms (internal)
	void AssumeFocus(void);

	void ResetActions(void);
	void RegisterAction(std::shared_ptr<ActionT> Action);

	std::unique_ptr<ReactionT> ReactionHandleInput(std::string const &ActionName, OptionalT<std::string> Text = {});

	std::string Dump(void) const;

	private:
		void Refresh(void);
};

}

#endif

