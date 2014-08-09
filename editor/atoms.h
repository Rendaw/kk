#ifndef atoms_h
#define atoms_h

#include "composite.h"

namespace Core
{
	
struct AssignmentTypeT : CompositeTypeT
{
	AssignmentTypeT(void);
};

struct ElementTypeT : CompositeTypeT
{
	ElementTypeT(void);
};

struct StringTypeT : CompositeTypeT
{
	StringTypeT(void);
};

struct GroupTypeT : CompositeTypeT
{
	GroupTypeT(void);
};

struct ModuleTypeT : CompositeTypeT
{
	ModuleTypeT(void);
};

}

#endif
