#include "protoatom.h"

#include "../shared/regex.h"

#include "composite.h"

namespace Core
{

ProtoatomTypeT::ProtoatomTypeT(void)
{
	Tag = "Protoatom";
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Lifted";
		//Part->SetDefault = true; // Unnecessary, since protoatom is only added explictly (no generic bubling sets or whatnot)
		Part->StartEmpty = true;
		Parts.emplace_back(Part);
	}
	{
		auto Part = new ProtoatomPartTypeT(*this);
		Part->Tag = "Data";
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}

NucleusT *ProtoatomPartTypeT::Generate(CoreT &Core)
{
	return new ProtoatomPartT(Core, *this);
}

ProtoatomPartT::ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo)
{
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
}

void ProtoatomPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);
	if (IsIdentifier) Polymorph.Bool("IsIdentifier", *IsIdentifier);
	Polymorph.String("Data", Data);
	Polymorph.UInt("Position", Position);
}

AtomTypeT const &ProtoatomPartT::GetTypeInfo(void) const { return TypeInfo; }
	
void ProtoatomPartT::Focus(FocusDirectionT Direction)
{
	if ((Direction == FocusDirectionT::FromBehind) ||
		(Direction == FocusDirectionT::Direct))
	{
		Position = 0;
	}
	else if (Direction == FocusDirectionT::FromAhead)
	{
		Position = Data.size();
	}
	Focused = true;
	Visual.SetClass("flag-focused");
	FlagRefresh();
	NucleusT::Focus(Direction);
	for (auto &Atom : FocusDependents) Atom->FlagRefresh();
}

void ProtoatomPartT::Defocus(void) 
{
	Focused = false;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
	for (auto &Atom : FocusDependents) Atom->FlagRefresh();
}

void ProtoatomPartT::AssumeFocus(void)
{
	Focus(FocusDirectionT::Direct);
}

void ProtoatomPartT::Refresh(void)
{
	Visual.Start();
	if (Focused)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::HandleInput(InputT const &Input)
{
	struct SetT : ActionT
	{
		ProtoatomPartT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(ProtoatomPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Base, Base.Position, Base.Data);
			Base.Data = Data;
			Base.Position = Position;
			if (Data.empty()) Base.IsIdentifier.Unset();
			Base.FlagRefresh();
			return std::unique_ptr<ActionT>(Out);
		}
		
		bool Combine(std::unique_ptr<ActionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Data = Set->Data;
			return true;
		}
	};
	
	if (Input.Text)
	{
		auto Text = *Input.Text;

		static auto IdentifierClass = Regex::ParserT<>("[a-zA-Z0-9_]");
		auto NewIsIdentifier = IdentifierClass(Text);
		
		if (Text == " ") return Finish({}, Data);
		else if ((Text == "\n") || (Text == ";")) 
		{
			auto Actions = new ActionGroupT;
			auto Finished = Finish({}, Data);
			if (Finished) Actions->Add(std::move(*Finished));
			auto InputResult = Parent->HandleInput(InputT{InputT::MainT::NewStatement, {}});
			if (InputResult) Actions->Add(std::move(*InputResult));
			return std::unique_ptr<ActionT>(Actions);
		}
		else if (!IsIdentifier || (*IsIdentifier == NewIsIdentifier))
		{
			if (!IsIdentifier) IsIdentifier = NewIsIdentifier;
			auto NewData = Data;
			NewData.insert(Position, Text);
			auto NewPosition = Position + 1;
			if (NewPosition == NewData.size()) // Only do auto conversion if appending text
			{
				auto Found = Core.LookUpAtom(NewData);
				if (Found && Found->ReplaceImmediately) 
					return Finish(Found, NewData);
			}
			return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, NewData));
		}
		else if (IsIdentifier && (*IsIdentifier != NewIsIdentifier))
		{
			Assert(!Data.empty());
			auto Finished = Finish({}, Data);
			Assert(Finished);
			auto ActionGroup = new ActionGroupT();
			ActionGroup->Add(std::move(*Finished));
			ActionGroup->Add(Core.ActionHandleInput(Input));
			return std::unique_ptr<ActionT>(ActionGroup);
		}
	}
	else if (Input.Main)
	{
		switch (*Input.Main)
		{
			case InputT::MainT::Left: 
			{
				if (Position == 0) 
				{
					Parent->FocusPrevious();
				}
				else
				{
					Position -= 1;
					FlagRefresh();
				}
			} return {};
			case InputT::MainT::Right:
			{
				if (Position == Data.size()) 
				{
					Parent->FocusNext();
				}
				else
				{
					Position += 1;
					FlagRefresh();
				}
			} return {};
			case InputT::MainT::TextBackspace:
			{
				if (Position == 0) return {};
				auto NewData = Data;
				NewData.erase(Position - 1, 1);
				auto NewPosition = Position - 1;
				return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, NewData));
			} return {};
			case InputT::MainT::Delete:
			{
				if (Position == Data.size()) return {};
				auto NewData = Data;
				NewData.erase(Position, 1);
				return std::unique_ptr<ActionT>(new SetT(*this, Position, NewData));
			} return {};
			default: break;
		}
	}
	
	auto Result = Parent->HandleInput(Input);
	if (Result)
	{
		auto FinishResult = Finish({}, Data);
		if (FinishResult) 
		{
			auto ActionGroup = new ActionGroupT;
			ActionGroup->Add(std::move(*FinishResult));
			ActionGroup->Add(std::move(*Result));
			return std::unique_ptr<ActionT>(ActionGroup);
		}
		else return std::move(Result);
	}

	return {};
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::Finish(OptionalT<AtomTypeT *> Type, std::string Text)
{
	Assert(Parent->Atom);
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Type)
	{
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted)
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Replacement = Core.ProtoatomType->Generate(Core);
			auto Finished = Type->Generate(Core);
			Actions->Add(Replacement->Set(Finished));
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Replacement)));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : Core.ProtoatomType->Generate(Core);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Child)));
			AtomT *WedgeAtom = PartParent()->Atom;
			while ((*WedgeAtom)->PartParent() && 
				(
					((*WedgeAtom)->PartParent()->GetTypeInfo().Precedence > (*Type)->Precedence) || 
					(
						((*WedgeAtom)->PartParent()->GetTypeInfo().Precedence == (*Type)->Precedence) &&
						((*WedgeAtom)->PartParent()->GetTypeInfo().LeftAssociative)
					)
				))
			{
				WedgeAtom = (*WedgeAtom)->PartParent()->Atom;
				Child = WedgeAtom->Nucleus;
			}
			auto Finished = (*Type)->Generate(Core);
			Assert(Finished);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*WedgeAtom, Finished)));
			Actions->Add(Finished->Set(Child));
		}
	}
	else
	{
		if (Lifted) 
		{
			if (Data.empty())
			{
				Assert(Atom);
				std::cout << "Replacing " << this << " with " << Lifted.Nucleus << ", atom " << Atom << std::endl;
				return std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Lifted.Nucleus));
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
		
		auto String = Core.StringType->Generate(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = Core.ProtoatomType->Generate(Core);

		auto ParentAsComposite = PartParent()->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType.get()))
		{
			Actions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(String));
		}
		else
		{
			auto Element = Core.ElementType->Generate(Core);
			Actions->Add(Element->As<CompositeT>()->Parts[1]->Set(String));
			Actions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(Element));
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Protoatom)));

		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ActionT>(Actions);
}

bool ProtoatomPartT::IsEmpty(void) const
{
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Data;
	return Data.empty() && !Lifted && !Focused;
}

}
