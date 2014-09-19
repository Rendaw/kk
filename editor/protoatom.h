#ifndef protoatom_h
#define protoatom_h

#include "core.h"
#include "composite.h"

namespace Core
{

struct ProtoatomPartT : NucleusT
{
	std::string Data;

	using NucleusT::NucleusT;
	virtual void HandleText(std::unique_ptr<UndoLevelT> &Level, std::string const &Text) = 0;
	virtual OptionalT<NucleusT *> Finish(std::unique_ptr<UndoLevelT> &Level, OptionalT<AtomTypeT *> Type, std::string Text) = 0;
};

struct ProtoatomT : CompositeT
{
	using CompositeT::CompositeT;
	virtual ProtoatomPartT *GetProtoatomPart(void) = 0;
};

struct SoloProtoatomT : ProtoatomT
{
	using ProtoatomT::ProtoatomT;
	bool IsEmpty(void);
	ProtoatomPartT *GetProtoatomPart(void);
};

struct WedgeProtoatomT : ProtoatomT
{
	using ProtoatomT::ProtoatomT;
	ProtoatomPartT *GetProtoatomPart(void);
	void SetLifted(NucleusT *Lifted); // Construction only
	AtomPartT *GetLiftedPart(void);
};

struct SoloProtoatomTypeT : CompositeTypeT 
{
	SoloProtoatomTypeT(void);
	NucleusT *Generate(CoreT &Core) override;
};

struct InsertProtoatomTypeT : CompositeTypeT
{
	InsertProtoatomTypeT(void);
	NucleusT *Generate(CoreT &Core) override;
};

struct AppendProtoatomTypeT : CompositeTypeT
{
	AppendProtoatomTypeT(void);
	NucleusT *Generate(CoreT &Core) override;
};

}

#endif
