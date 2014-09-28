#ifndef core_h
#define core_h

#include <functional>
#include <list>
#include <set>
#include <vector>
#include <memory>
#include <regex>
#include <QWebElement>

#include "../ren-cxx-basics/type.h"
#include "../ren-cxx-serialjson/serial.h"

namespace Core
{

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
		
struct TOCLocationT
{
	std::list<size_t> Orders;
	size_t Level;

	bool operator <(TOCLocationT const &Other) const;
	bool Contains(TOCLocationT const &Other) const;
	std::string Dump(void *Pointer) const;
};

struct TOCVisualT
{
	virtual ~TOCVisualT(void);
	virtual void SetText(std::string const &NewText) = 0;
	virtual void SetLocation(OptionalT<TOCLocationT> &&Location) = 0;
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
	CoreT &Core;

	HoldT(CoreT &Core);
	HoldT(CoreT &Core, NucleusT *Nucleus);
	HoldT(HoldT const &Other);
	~HoldT(void);
	
	HoldT &operator =(NucleusT *Nucleus);
	HoldT &operator =(HoldT &&Hold);
	void Set(NucleusT *Nucleus);
	void Clear(void);
	
	operator bool(void) const;

	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	NucleusT *Nucleus(void);
	NucleusT const *Nucleus(void) const;
	
	private:
		NucleusT *Nucleus_;
};

struct AtomT
{
	CoreT &Core;

	typedef std::function<void(NucleusT *Nucleus)> AtomCallbackT;
	AtomCallbackT Callback;
	NucleusT *Parent;

	AtomT(AtomT const &Other) = delete;
	AtomT(CoreT &Core);
	~AtomT(void);
	
	void Set(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus);

	operator bool(void) const;

	NucleusT *operator ->(void);
	NucleusT const *operator ->(void) const;
	NucleusT *Nucleus(void);
	NucleusT const *Nucleus(void) const;

	size_t Order(void) const;
	void SetOrder(size_t Ordering);
	
	private:
		void Clear(void);

		size_t Order_;
		NucleusT *Nucleus_;
};

enum struct FocusDirectionT
{
	FromAhead,
	FromBehind,
	Direct
};

struct AtomTypeT;

struct NucleusReduction1T
{
	NucleusReduction1T(CoreT &Core);

	NucleusT *Parent(void);
	NucleusT const *Parent(void) const;
	AtomT *Atom(void);
	AtomT const *Atom(void) const;

	private:
		friend struct AtomT;
		friend struct HoldT;
		friend struct CoreT;
		HoldT Parent_;
		size_t Count = 0;
		AtomT *Atom_;
};

struct NucleusT : NucleusReduction1T
{
	CoreT &Core;
	VisualT Visual;

	NucleusT(CoreT &Core);
	virtual ~NucleusT(void);

	// Global use
	OptionalT<NucleusT *> PartParent(void);

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
	
	OptionalT<std::list<size_t>> GetGlobalOrder(void) const;
	virtual AtomTypeT const &GetTypeInfo(void) const;
	
	// Core use
	virtual Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) = 0;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const;
	virtual void Serialize(Serial::WritePolymorphT &Polymorph) const;
	virtual void RegisterActions(void) = 0;

	// Tree use
	virtual void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction);
	virtual void FocusPrevious(std::unique_ptr<UndoLevelT> &Level);
	virtual void FocusNext(std::unique_ptr<UndoLevelT> &Level);
	virtual void AlignFocus(NucleusT *Child);
	virtual void AssumeFocus(std::unique_ptr<UndoLevelT> &Level);
	virtual bool IsFocused(void) const;
	virtual bool IsFramed(void) const;
		
	void WatchStatus(uintptr_t ID, std::function<void(NucleusT *Changed)> Callback);
	void IgnoreStatus(uintptr_t ID);
	
	// Callbacks (1 specific user)
	virtual void Defocus(std::unique_ptr<UndoLevelT> &Level);
	virtual void LocationChanged(void);
	virtual void ViewChanged(bool Framed, size_t Depth);
	virtual void Refresh(void);

	// Self use
	void FlagRefresh(void);
	void FlagStatusChange(void);
	
	private:
		std::map<uintptr_t, std::function<void(NucleusT *Changed)>> StatusWatchers;
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

struct CoreBaseT
{
	CoreBaseT(CoreT &Core, VisualT &RootVisual);
		
	struct SettingsT
	{
		bool UnframeAtRoot = true;
		bool StartFramed = false;
		OptionalT<size_t> FrameDepth;
	};
	SettingsT const &Settings(void) const;

	NucleusT *Focused(void);
	NucleusT const *Focused(void) const;

	bool TextMode;

	protected:
		CoreT &This; // Hack

		AtomT Root;
		HoldT Focused_;
		VisualT &RootVisual;
		bool RootRefresh;
		bool NeedScroll;
		
		SettingsT Settings_;

		std::map<std::string, std::unique_ptr<AtomTypeT>> Types;
		
		Serial::ReadErrorT Deserialize(std::unique_ptr<UndoLevelT> &Level, AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object);
};

struct CoreReduction1T : virtual CoreBaseT
{
	CoreReduction1T(void);

	std::function<std::unique_ptr<TOCVisualT>(NucleusT *Owner)> CreateTOCVisual;

	friend struct CoreT;
	friend struct NucleusT;
	friend struct CompositeT;
	friend struct AtomPartT;
	friend struct AtomListPartT;
	friend struct StringPartT;
	friend struct EnumPartT;
	friend struct ProtoatomPartT;
	friend struct BaseProtoatomPartT;
	friend struct SoloProtoatomPartT;
	friend struct WedgeProtoatomPartT;
	friend OptionalT<NucleusT *> TypedFinish(std::unique_ptr<UndoLevelT> &Level, CoreT &Core, bool Bubble, bool Insert, AtomTypeT &Type, AtomT *ParentAtom, OptionalT<NucleusT *> Set);
	private:
		VisualT CursorVisual;
		AtomTypeT *SoloProtoatomType, *InsertProtoatomType, *AppendProtoatomType, *ElementType, *StringType;
};

struct CoreReduction2_1T : virtual CoreBaseT
{
	CoreReduction2_1T(CoreT &Core);

	friend struct CoreReduction2T;
	friend struct NucleusT;
	friend struct CoreT;
	private:
		HoldT Framed;
};

struct CoreReduction2T : CoreReduction2_1T, virtual CoreReduction1T
{
	CoreReduction2T(CoreT &Core);

	std::function<void(void)> FocusChangedCallback;
	std::function<void(std::string &&Text)> CopyCallback;
	std::function<std::unique_ptr<std::stringstream>(void)> PasteCallback;

	bool IsFramed(void) const;

	friend struct CoreT;
	friend struct NucleusT;
	friend struct CompositeT;
	private:
		std::set<NucleusT *> NeedRefresh;

		void Focus(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus);
		void Frame(NucleusT *Nucleus);
		void Copy(NucleusT *Tree);
		void Paste(std::unique_ptr<UndoLevelT> &UndoLevel, AtomT &Destination);
};

struct CoreReduction3T : virtual CoreBaseT
{
	CoreReduction3T(void);

	friend struct CoreT;
	friend struct AtomT;
	friend struct HoldT;
	private:
		std::set<NucleusT *> DeletionCandidates;
};

struct CoreReduction4_1T : virtual CoreBaseT
{
	CoreReduction4_1T(void);

	friend struct CoreT;
	friend struct CoreReduction4T;
	private:
		std::map<std::string, AtomTypeT *> TypeLookup;
};

struct CoreReduction4T : CoreReduction4_1T
{
	CoreReduction4T(void);

	friend struct CoreT;
	friend struct ProtoatomPartT;
	friend struct BaseProtoatomPartT;
	friend struct SoloProtoatomPartT;
	friend struct WedgeProtoatomPartT;
	private:
		OptionalT<AtomTypeT *> LookUpAtomType(std::string const &Text);
		bool CouldBeAtomType(std::string const &Text);
};

struct CoreReduction5T : virtual CoreBaseT
{
	CoreReduction5T(void);

	friend struct CoreT;
	friend struct AtomT;
	friend struct CompositeT;
	private:
		bool UndoingOrRedoing; // Debugging aid
};

struct CoreT : virtual CoreReduction1T, CoreReduction2T, CoreReduction3T, CoreReduction4T, CoreReduction5T
{
	// External use only
	CoreT(VisualT &RootVisual);
	~CoreT(void);
	Serial::ReadErrorT Configure(Serial::ReadObjectT &Object);
	Serial::ReadErrorT ConfigureFinished(void);
	void Serialize(Filesystem::PathT const &Path);
	void Deserialize(Filesystem::PathT const &Path);
	
	void HandleInput(std::shared_ptr<ActionT> Action);
	
	bool HasChanges(void);
	
	void Focus(NucleusT *Nucleus);

	std::function<void(void)> ResetActionsCallback;
	std::function<void(std::shared_ptr<ActionT> Action)> RegisterActionCallback;
	std::function<void(void)> HandleInputFinishedCallback;

	// Anyone's use
	void RegisterAction(std::shared_ptr<ActionT> Action);

	std::string Dump(void) const;

	private:
		VisualT FrameVisual;
		
		std::list<std::shared_ptr<ActionT>> Actions;

		std::list<std::unique_ptr<UndoLevelT>> UndoQueue, RedoQueue;
	
		void AssumeFocus(std::unique_ptr<UndoLevelT> &Level);
		void Refresh(void);
		void ResetActions(void);
		void RegisterActions(void);
};

}

#endif

