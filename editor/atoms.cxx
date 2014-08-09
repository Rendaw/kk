#include "atoms.h"

namespace Core
{

AssignmentTypeT::AssignmentTypeT(void)
{
	Tag = "Assignment";
	DisplayInfix = "=";
	ReplaceImmediately = true;
	Arity = ArityT::Binary;
	Prefix = false;
	Precedence = 900;
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Left";
		Parts.emplace_back(Part);
	}
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Right";
		Parts.emplace_back(Part);
	}
}

ElementTypeT::ElementTypeT(void)
{
	Tag = "Element";
	ReplaceImmediately = true;
	Arity = ArityT::Binary;
	Prefix = false;
	Precedence = 900;
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Base";
		Part->DisplaySuffix = ".";
		Part->SetDefault = true;
		Parts.emplace_back(Part);
	}
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Key";
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}

StringTypeT::StringTypeT(void)
{
	Tag = "String";
	ReplaceImmediately = true;
	Arity = ArityT::Nullary;
	Prefix = true;
	{
		auto Part = new StringPartTypeT(*this);
		Part->Tag = "Data";
		Part->DisplayPrefix = "'";
		Part->DisplaySuffix = "'";
		Part->SetDefault = true;
		Parts.emplace_back(Part);
	}
}

GroupTypeT::GroupTypeT(void)
{
	Tag = "Group";
	DisplayPrefix = "{";
	DisplaySuffix = "}";
	SpatiallyVertical = true;
	{
		auto Part = new AtomListPartTypeT(*this);
		Part->Tag = "Statements";
		Part->DisplaySuffix = ";";
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}

ModuleTypeT::ModuleTypeT(void)
{
	Tag = "Module";
	SpatiallyVertical = true;
	{
		auto Part = new EnumPartTypeT(*this);
		Part->Tag = "Entry";
		Part->Values.push_back("Entry Point");
		Part->Values.push_back("No Entry Point");
		Parts.emplace_back(Part);
	}
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Top";
		Part->SetDefault = true;
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}
	
}
