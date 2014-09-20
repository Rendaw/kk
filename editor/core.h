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

struct NucleusT;
struct VisualT
{
	VisualT(QWebElement const &Element);
	VisualT(QWebElement const &Root, QWebElement const &Element);
	VisualT(VisualT const &) = delete;
	~VisualT(void);

	void SetFocusTarget(NucleusT const *Nucleus);

	void SetClass(std::string const &Class);
	void UnsetClass(std::string const &Class);
	VisualT &Tag(void);

	void Start(void);
	void Add(VisualT &Other);
	void Add(std::string const &Text);
	void Set(std::string const &Text);
	std::string Dump(void);

	void Scroll(void);
	void ScrollToTop(void);

	QWebElement Root;
	
	private:
		std::string const ID;
		std::string IDName(void);
	
		std::unique_ptr<VisualT> TagVisual;
};

struct UndoLevelT;
struct ReactionT
{
	virtual ~ReactionT(void);
	virtual void Apply(std::unique_ptr<UndoLevelT> &Level) = 0;
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
	virtual void Apply(std::unique_ptr<UndoLevelT> &Level) = 0;
};

struct FunctionActionT : ActionT
{
	std::function<void(std::unique_ptr<UndoLevelT> &Level)> Function;
	FunctionActionT(std::string const &Name, std::function<void(std::unique_ptr<UndoLevelT> &Level)> const &Function);
	void Apply(std::unique_ptr<UndoLevelT> &Level) override;
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
	AtomT(AtomT const &Other) = delete;
	AtomT(CoreT &Core);
	~AtomT(void);
	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	operator bool(void) const;

	void Set(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus);
	
	typedef std::function<void(NucleusT *Nucleus)> AtomCallbackT;
	CoreT &Core;
	AtomCallbackT Callback;
	NucleusT *Parent;
	NucleusT *Nucleus;

	private:
		void Clear(void);
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
	virtual void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction);
	virtual void AlignFocus(NucleusT *Child);
	virtual void RegisterActions(void) = 0;
	virtual void Defocus(std::unique_ptr<UndoLevelT> &Level);
	virtual void AssumeFocus(std::unique_ptr<UndoLevelT> &Level); // If bool is false, optional must not be set
	virtual void Refresh(void);

	virtual void FocusPrevious(std::unique_ptr<UndoLevelT> &Level);
	virtual void FocusNext(std::unique_ptr<UndoLevelT> &Level);
	
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

	size_t OperatorPosition = 0;
	OptionalT<std::string> Operator;

	bool ReplaceImmediately = false;
	int Precedence = 1000;
	bool LeftAssociative = true;
	
	bool SpatiallyVertical = false;
};

struct FocusT : ReactionT
{
	FocusT(CoreT &Core, NucleusT *Nucleus, bool DoNothing = false);

	friend struct CoreT;
	protected:
		void Apply(std::unique_ptr<UndoLevelT> &Level);

	private:
		CoreT &Core;
		HoldT Target;
		bool DoNothing;
};

struct UndoLevelT
{
	std::list<std::unique_ptr<ReactionT>> Reactions;

	void Add(std::unique_ptr<ReactionT> Reaction);
	void ApplyUndo(std::unique_ptr<UndoLevelT> &Level);
	void ApplyRedo(std::unique_ptr<UndoLevelT> &Level);
	bool Combine(std::unique_ptr<UndoLevelT> &Other);
};

struct CoreT
{
	VisualT &RootVisual;
	AtomT Root;
	HoldT Focused;
	
	bool TextMode;
	
	std::map<std::string, std::unique_ptr<AtomTypeT>> Types;
	AtomTypeT *SoloProtoatomType, *InsertProtoatomType, *AppendProtoatomType, *ElementType, *StringType;
	std::map<std::string, AtomTypeT *> TypeLookup;

	std::set<NucleusT *> DeletionCandidates;
	std::set<NucleusT *> NeedRefresh;
	bool NeedScroll;

	VisualT CursorVisual;

	bool UndoingOrRedoing; // Debugging aide

	// External interface
	std::list<std::shared_ptr<ActionT>> Actions;
	std::function<void(void)> ResetActionsCallback;
	std::function<void(std::shared_ptr<ActionT> Action)> RegisterActionCallback;
	std::function<void(std::string &&Text)> CopyCallback;
	std::function<std::unique_ptr<std::stringstream>(void)> PasteCallback;

	CoreT(VisualT &RootVisual);
	~CoreT(void);
	void Serialize(Filesystem::PathT const &Path);
	void Deserialize(Filesystem::PathT const &Path);
	
	void HandleInput(std::shared_ptr<ActionT> Action);
	
	bool HasChanges(void);
	
	void Focus(NucleusT *Nucleus);

	// Self + atom use
	Serial::ReadErrorT Deserialize(std::unique_ptr<UndoLevelT> &Level, AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object);
	OptionalT<AtomTypeT *> LookUpAtomType(std::string const &Text);

	void Copy(NucleusT *Tree);
	void Paste(std::unique_ptr<UndoLevelT> &UndoLevel, AtomT &Destination);

	void Focus(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus);
	void Refresh(void);

	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level);

	void ResetActions(void);
	void RegisterAction(std::shared_ptr<ActionT> Action);

	std::string Dump(void) const;

	private:
		std::list<std::unique_ptr<UndoLevelT>> UndoQueue, RedoQueue;
};

}

#endif

