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
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override;
	std::unique_ptr<ActionT> Set(std::string const &Text) override;
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
	
	bool FocusDefault(void);
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
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override;
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
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
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void Add(size_t Position, NucleusT *Nucleus, bool ShouldFocus = false);
	void Remove(size_t Position);
	
	struct AddRemoveT : ActionT
	{
		AtomListPartT &Base;
		bool Add;
		size_t Position;
		HoldT Nucleus;

		AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus);

		std::unique_ptr<ActionT> Apply(void);
	};

	std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override;
	std::unique_ptr<ActionT> Set(std::string const &Text) override;
	
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
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
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	
	std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override;
	
	struct SetT : ActionT
	{
		StringPartT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(StringPartT &Base, unsigned int Position, std::string const &Data);
		
		std::unique_ptr<ActionT> Apply(void);
		bool Combine(std::unique_ptr<ActionT> &Other) override;
	};

	std::unique_ptr<ActionT> Set(std::string const &Text) override;

	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
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
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;

	struct SetT : ActionT
	{
		EnumPartT &Base;
		size_t Index;

		SetT(EnumPartT &Base, size_t Index);
		std::unique_ptr<ActionT> Apply(void);
	};
	
	std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override;
	std::unique_ptr<ActionT> Set(std::string const &Text) override;

	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
};

}

#endif
