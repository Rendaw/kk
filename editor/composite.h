#ifndef composite_h
#define composite_h

#include "core.h"

namespace Core
{

enum struct OperatorDirectionT
{
	Left,
	Right
};

struct CompositeTypeT;

struct CompositeT : NucleusT
{
	CompositeTypeT &TypeInfo;
	VisualT OperatorVisual;

	struct SelfFocusedT {};
	typedef size_t PartFocusedT;
	VariantT<SelfFocusedT, PartFocusedT> Focused;
	
	bool EffectivelyVertical;
	bool Ellipsized;

	std::vector<std::unique_ptr<AtomT>> Parts;

	CompositeT(CoreT &Core, CompositeTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) override;
	void AlignFocus(NucleusT *Child) override;
	void FrameDepthAdjusted(OptionalT<size_t> Depth) override;
	void RegisterActions(void) override;
	void Defocus(std::unique_ptr<UndoLevelT> &Level) override;
	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level) override;
	void Refresh(void) override;
	void FocusPrevious(std::unique_ptr<UndoLevelT> &Level) override;
	void FocusNext(std::unique_ptr<UndoLevelT> &Level) override;
	bool IsFocused(void) const override;
	
	bool FocusDefault(std::unique_ptr<UndoLevelT> &Level);

	OptionalT<AtomT *> GetOperand(OperatorDirectionT Direction, size_t Offset);
	OptionalT<NucleusT *> GetAnyOperand(OperatorDirectionT Direction, size_t Offset);
	OptionalT<OperatorDirectionT> GetOperandSide(AtomT *Operand);
};

struct CompositePartTypeT;

struct CompositeTypeT : AtomTypeT
{
	bool Ellipsize = false;

	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object);
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
	
	bool IsAssociative(OperatorDirectionT Direction) const;

	std::vector<std::unique_ptr<CompositePartTypeT>> Parts;
};

struct CompositePartTypeT : AtomTypeT
{
	CompositeTypeT &Parent;
	
	bool FocusDefault = false;

	CompositePartTypeT(CompositeTypeT &Parent);
	virtual Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object);
	virtual void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const = 0;
	void Serialize(Serial::WriteObjectT &Object) const override;
};

struct OperatorPartTypeT : CompositePartTypeT
{
	std::string Pattern;
	
	using CompositePartTypeT::CompositePartTypeT;
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
};

template <typename CompositeDerivateT> NucleusT *GenerateComposite(CompositeTypeT &Type, CoreT &Core)
{
	auto DiscardUndoLevel = make_unique<UndoLevelT>();
	auto Out = new CompositeDerivateT(Core, Type);
	for (auto &Part : Type.Parts) 
	{
		if (dynamic_cast<OperatorPartTypeT *>(Part.get())) continue;
		Out->Parts.push_back(make_unique<AtomT>(Core));
		Out->Parts.back()->Parent = Out;
		Out->Parts.back()->Set(DiscardUndoLevel, Part->Generate(Core));
		auto &CapturePart = *Out->Parts.back().get();
		Out->Parts.back()->Callback = [&CapturePart, Out, &Core](NucleusT *Replacement) 
		{ 
			// Parts can't be replaced, except during undo or redo
			Assert(Core.UndoingOrRedoing);
			if (CapturePart) CapturePart->IgnoreStatus((uintptr_t)Out);
			if (Replacement) Replacement->WatchStatus((uintptr_t)Out, [](NucleusT *Nucleus) { Nucleus->Parent->FlagStatusChange(); });
		}; 
		(*Out->Parts.back())->WatchStatus((uintptr_t)Out, [](NucleusT *Nucleus) { Nucleus->Parent->FlagStatusChange(); });
	}
	return Out;
}

struct AtomPartTypeT : CompositePartTypeT
{
	bool StartEmpty = false;
	
	using CompositePartTypeT::CompositePartTypeT;
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
};
struct AtomPartT : NucleusT
{
	AtomPartTypeT &TypeInfo;
	
	bool EffectivelyVertical;
	
	AtomT Data;

	AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) override;
	void FrameDepthAdjusted(OptionalT<size_t> Depth) override;
	void RegisterActions(void) override;
	void Defocus(std::unique_ptr<UndoLevelT> &Level) override;
	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level) override;
	void Refresh(void) override;
	void FocusPrevious(std::unique_ptr<UndoLevelT> &Level) override;
	void FocusNext(std::unique_ptr<UndoLevelT> &Level) override;
};

struct AtomListPartTypeT : CompositePartTypeT
{
	using CompositePartTypeT::CompositePartTypeT;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	using CompositePartTypeT::Serialize; // Is this funny?
	NucleusT *Generate(CoreT &Core) override;
};
struct AtomListPartT : NucleusT
{
	AtomListPartTypeT &TypeInfo;

	size_t FocusIndex;
	
	bool EffectivelyVertical;

	struct ItemT
	{
		VisualT Visual;
		AtomT Atom;
	};
	std::vector<std::unique_ptr<ItemT>> Data;

	AtomListPartT(CoreT &Core, AtomListPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) override;
	void AlignFocus(NucleusT *Child) override;
	void FrameDepthAdjusted(OptionalT<size_t> Depth) override;
	void RegisterActions(void) override;
	void Defocus(std::unique_ptr<UndoLevelT> &Level) override;
	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level) override;
	void Refresh(void) override;
	void Add(std::unique_ptr<UndoLevelT> &Level, size_t Position, NucleusT *Nucleus, bool ShouldFocus = false);
	void Remove(std::unique_ptr<UndoLevelT> &Level, size_t Position);

	void FocusPrevious(std::unique_ptr<UndoLevelT> &Level) override;
	void FocusNext(std::unique_ptr<UndoLevelT> &Level) override;

	private:
		struct AddRemoveT : ReactionT
		{
			AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus);

			void Apply(std::unique_ptr<UndoLevelT> &Level);

			AtomListPartT &Base;
			bool Add;
			size_t Position;
			HoldT Nucleus;
		};
};

struct StringPartTypeT : CompositePartTypeT
{
	using CompositePartTypeT::CompositePartTypeT;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	using CompositePartTypeT::Serialize; // Is this funny?
	NucleusT *Generate(CoreT &Core) override;
};
struct StringPartT : NucleusT
{
	StringPartTypeT &TypeInfo;
	enum struct FocusedT
	{
		Off,
		On,
		Text
	} Focused;
	size_t Position;
	std::string Data;

	StringPartT(CoreT &Core, StringPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(std::unique_ptr<UndoLevelT> &Level) override;
	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level) override;
	void Refresh(void) override;
	
	void Set(std::unique_ptr<UndoLevelT> &Level, size_t Position, std::string const &Text);
};

struct EnumPartTypeT : CompositePartTypeT
{
	std::vector<std::string> Values;
	std::map<std::string, size_t> ValueLookup;
	
	using CompositePartTypeT::CompositePartTypeT;
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
};
struct EnumPartT : NucleusT
{
	EnumPartTypeT &TypeInfo;

	size_t Index;

	EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(std::unique_ptr<UndoLevelT> &Level) override;
	void AssumeFocus(std::unique_ptr<UndoLevelT> &Level) override;
	void Refresh(void) override;
};

void CheckStringType(AtomTypeT *Type);
StringPartT *GetStringPart(NucleusT *Nucleus);

void CheckElementType(AtomTypeT *Type);
AtomPartT *GetElementLeftPart(NucleusT *Nucleus);
AtomPartT *GetElementRightPart(NucleusT *Nucleus);

bool IsPrecedent(AtomT &ParentAtom, NucleusT *Child);

}

#endif
