#ifndef protoatom_h
#define protoatom_h

#include "core.h"
#include "composite.h"

namespace Core
{

struct ProtoatomPartTypeT : CompositeTypePartT
{
	using CompositeTypePartT::CompositeTypePartT;
	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const override;
	using CompositeTypePartT::Serialize; // Is this funny?
	NucleusT *Generate(CoreT &Core) override;
};
struct ProtoatomPartT : NucleusT
{
	ProtoatomPartTypeT &TypeInfo;
	
	OptionalT<bool> IsIdentifier;
	std::string Data;
	enum struct FocusedT
	{
		Off,
		On,
		Text
	} Focused = FocusedT::Off;
	size_t Position = 0;

	ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo);
	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override;
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	AtomTypeT const &GetTypeInfo(void) const override;
	void Focus(FocusDirectionT Direction) override;
	void RegisterActions(void) override;
	void Defocus(void) override;
	void AssumeFocus(void) override;
	void Refresh(void) override;
	
	bool IsEmpty(void) const override;

	void Set(size_t Position, std::string const &Text);
	void HandleText(std::string const &Text);

	OptionalT<NucleusT *> Finish(OptionalT<AtomTypeT *> Type, std::string Text);
};

void CheckProtoatomType(AtomTypeT *Type);

template <typename RefT> static OptionalT<ProtoatomPartT *> AsProtoatom(RefT &Test)
{
	auto Original = Test->template As<CompositeT>();
	if (Original && (Original->Parts.size() == 2)) 
		return Original->Parts[1]->template As<ProtoatomPartT>();
	return {};
}

ProtoatomPartT *GetProtoatomPart(NucleusT *Protoatom);
AtomPartT *GetProtoatomLiftedPart(NucleusT *Protoatom);

}

#endif
