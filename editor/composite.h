#ifndef composite_h
#define composite_h

#include "core.h"

namespace Core
{

struct CompositeTypeT;

struct CompositeT : NucleusT
{
	CompositeTypeT &TypeInfo;
	VisualT PrefixVisual, InfixVisual, SuffixVisual;

	struct SelfFocusedT {};
	typedef size_t PartFocusedT;
	VariantT<SelfFocusedT, PartFocusedT> Focused;
	
	bool EffectivelyVertical;

	std::vector<AtomT> Parts;
	
	CompositeT(CoreT &Core, CompositeTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
	bool IsEmpty(void) const override;
	bool IsFocused(void) const override;
	
	bool FocusDefault(void);

	bool HasOnePart(void); // Only one part exists or is non-empty
};

struct CompositeTypePartT;

struct CompositeTypeT : AtomTypeT
{
	// Configurable
	OptionalT<std::string> DisplayPrefix, DisplayInfix, DisplaySuffix;

	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object);
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;

	std::vector<std::unique_ptr<CompositeTypePartT>> Parts;
};

struct CompositeTypePartT : AtomTypeT
{
	CompositeTypeT &Parent;
	
	// Unconfigurable
	bool FocusDefault = false;
	bool SetDefault = false;

	// Configurable
	OptionalT<std::string> DisplayPrefix, DisplaySuffix;

	CompositeTypePartT(CompositeTypeT &Parent);
	virtual Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object);
	virtual void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const = 0;
	void Serialize(Serial::WriteObjectT &Object) const override;
};

struct AtomPartTypeT : CompositeTypePartT
{
	bool StartEmpty = false;
	
	using CompositeTypePartT::CompositeTypePartT;
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
};
struct AtomPartT : NucleusT
{
	AtomPartTypeT &TypeInfo;
	VisualT PrefixVisual, SuffixVisual;
	
	bool EffectivelyVertical;
	
	AtomT Data;

	AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
	bool IsEmpty(void) const override;
};

struct AtomListPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	using CompositeTypePartT::Serialize; // Is this funny?
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
		VisualT PrefixVisual, SuffixVisual;
		AtomT Atom;
	};
	std::vector<std::unique_ptr<ItemT>> Data;

	AtomListPartT(CoreT &Core, AtomListPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void Add(size_t Position, NucleusT *Nucleus, bool ShouldFocus = false);
	void Remove(size_t Position);

	void FocusPrevious(void) override;
	void FocusNext(void) override;

	private:
		struct AddRemoveT : ReactionT
		{
			AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus);

			void Apply(void);

			AtomListPartT &Base;
			bool Add;
			size_t Position;
			HoldT Nucleus;
		};
};

struct StringPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	using CompositeTypePartT::Serialize; // Is this funny?
	NucleusT *Generate(CoreT &Core) override;
};
struct StringPartT : NucleusT
{
	StringPartTypeT &TypeInfo;
	VisualT PrefixVisual, SuffixVisual;
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
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	
	void Set(size_t Position, std::string const &Text);
};

struct EnumPartTypeT : CompositeTypePartT
{
	std::vector<std::string> Values;
	std::map<std::string, size_t> ValueLookup;
	
	using CompositeTypePartT::CompositeTypePartT;
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	void Serialize(Serial::WriteObjectT &Object) const override;
	NucleusT *Generate(CoreT &Core) override;
};
struct EnumPartT : NucleusT
{
	EnumPartTypeT &TypeInfo;
	VisualT PrefixVisual, SuffixVisual;

	size_t Index;

	EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
};

void CheckStringType(AtomTypeT *Type);
StringPartT *GetStringPart(NucleusT *Nucleus);

void CheckElementType(AtomTypeT *Type);
AtomPartT *GetElementLeftPart(NucleusT *Nucleus);
AtomPartT *GetElementRightPart(NucleusT *Nucleus);

}

#endif
