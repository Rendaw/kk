#ifndef protoatom_h
#define protoatom_h

#include "core.h"
#include "composite.h"

namespace Core
{

struct HoldPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	NucleusT *Generate(CoreT &Core) override;
};
struct HoldPartT : NucleusT
{
	HoldPartTypeT &TypeInfo;
	VisualT PrefixVisual, SuffixVisual;
	
	HoldT Data;

	HoldPartT(CoreT &Core, HoldPartTypeT &TypeInfo);

	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	void SetHold(NucleusT *Nucleus);
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	void FocusPrevious(void) override;
	void FocusNext(void) override;
};

struct ProtoatomTypeT : CompositeTypeT
{
	ProtoatomTypeT(void);
};

struct ProtoatomPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	NucleusT *Generate(CoreT &Core) override;
};
struct ProtoatomPartT : NucleusT
{
	ProtoatomPartTypeT &TypeInfo;
	
	OptionalT<bool> IsIdentifier;
	std::string Data;
	bool Focused;
	size_t Position = 0;

	std::vector<HoldT> FocusDependents;

	ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo);
	
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override;
	
	OptionalT<std::unique_ptr<ActionT>> Finish(
		OptionalT<AtomTypeT *> Type, 
		OptionalT<std::string> NewData, 
		OptionalT<InputT> SeedData);
	
	bool IsEmpty(void) const;
};

template <typename RefT> static OptionalT<ProtoatomPartT *> AsProtoatom(RefT &Test)
{
	auto Original = Test->template As<CompositeT>();
	if (Original && (Original->Parts.size() == 2)) 
		return Original->Parts[1]->template As<ProtoatomPartT>();
	return {};
}

}

#endif
