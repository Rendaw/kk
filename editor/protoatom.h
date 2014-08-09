#ifndef protoatom_h
#define protoatom_h

#include "core.h"

namespace Core
{

struct ProtoatomT : NucleusT
{
	OptionalT<bool> IsIdentifier;
	std::string Data;
	bool Focused;
	size_t Position = 0;

	HoldT Lifted;

	ProtoatomT(CoreT &Core);
	
	void Serialize(Serial::WritePolymorphT &Polymorph) const override;
	
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
