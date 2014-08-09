#ifndef composite_h
#define composite_h

#include "core.h"

namespace Core
{

struct CompositeTypeT;

struct CompositeT : NucleusT
{
	CompositeTypeT &TypeInfo;

	struct SelfFocusedT {};
	typedef size_t PartFocusedT;
	VariantT<SelfFocusedT, PartFocusedT> Focused;

	std::vector<AtomT> Parts;

	CompositeT(CoreT &Core, CompositeTypeT &TypeInfo);

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
};

struct CompositeTypePartT;

struct CompositeTypeT : AtomTypeT
{
	// Configurable
	bool SpatiallyVertical = false;
	OptionalT<std::string> DisplayPrefix, DisplayInfix, DisplaySuffix;

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
};

struct AtomPartT;
struct AtomPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	NucleusT *Generate(CoreT &Core) override;
};
struct AtomPartT : NucleusT
{
	AtomPartTypeT &TypeInfo;
	AtomT Data;

	AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo);

	void Parented(void) override;

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
};

struct AtomListPartT;
struct AtomListPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	NucleusT *Generate(CoreT &Core) override;
};
struct AtomListPartT : NucleusT
{
	AtomListPartTypeT &TypeInfo;

	size_t FocusIndex;

	struct ItemT
	{
		VisualT Visual;
		AtomT Atom;
	};
	std::vector<std::unique_ptr<ItemT>> Data;

	AtomListPartT(CoreT &Core, AtomListPartTypeT &TypeInfo);

	void Parented(void) override;

	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void Add(size_t Position, NucleusT *Nucleus);
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
	NucleusT *Generate(CoreT &Core) override;
};
struct StringPartT : NucleusT
{
	StringPartTypeT &TypeInfo;
	bool Focused;
	size_t Position;
	std::string Data;

	StringPartT(CoreT &Core, StringPartTypeT &TypeInfo);

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
	
	using CompositeTypePartT::CompositeTypePartT;
	NucleusT *Generate(CoreT &Core) override;
};
struct EnumPartT : NucleusT
{
	EnumPartTypeT &TypeInfo;

	size_t Index;

	EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo);
	
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
