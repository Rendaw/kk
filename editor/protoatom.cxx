#include "protoatom.h"

#include "../shared/regex.h"

#include "logging.h"
#include "composite.h"

namespace Core
{

ProtoatomTypeT::ProtoatomTypeT(void)
{
	TRACE;
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
	
void ProtoatomPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("ProtoatomPart", std::move(Prepolymorph));
	Serialize(Polymorph);
}

NucleusT *ProtoatomPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new ProtoatomPartT(Core, *this);
}

ProtoatomPartT::ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
}

Serial::ReadErrorT ProtoatomPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.Bool("IsIdentifier", [this](bool Value) -> Serial::ReadErrorT { IsIdentifier = Value; return {}; });
	Object.String("Data", [this](std::string &&Value) -> Serial::ReadErrorT { Data = std::move(Value); return {}; });
	Object.UInt("Position", [this](uint64_t Value) -> Serial::ReadErrorT { Position = Value; return {}; }); // Apparently you can assign a uint64_t to a string, with no errors or warnings
	return {};
}

void ProtoatomPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE;
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	if (IsIdentifier) Polymorph.Bool("IsIdentifier", *IsIdentifier);
	Polymorph.String("Data", Data);
	Polymorph.UInt("Position", Position);
}

AtomTypeT const &ProtoatomPartT::GetTypeInfo(void) const { return TypeInfo; }
	
void ProtoatomPartT::Focus(FocusDirectionT Direction)
{
	TRACE;
	if (Parent->As<CompositeT>()->HasOnePart()) Core.TextMode = true;
	if (Core.TextMode)
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
		Focused = FocusedT::Text;
	}
	else
	{
		Visual.SetClass("flag-focused");
		Focused = FocusedT::On;
	}
	FlagRefresh();
	FlagStatusChange();
	NucleusT::Focus(Direction);
	if (Parent->Parent && Parent->Parent->As<AtomPartT>())
		Parent->Parent->FlagRefresh(); // Hack for hiding empty protoatoms
}
	
void ProtoatomPartT::RegisterActions(void)
{
	TRACE;
	
	static auto IdentifierClass = Regex::ParserT<>("^[a-zA-Z0-9_]+$");

	struct SetT : ReactionT
	{
		ProtoatomPartT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(ProtoatomPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
		
		std::unique_ptr<ReactionT> Apply(void)
		{
			TRACE;
			auto Out = new SetT(Base, Base.Position, Base.Data);
			Base.Data = Data;
			Base.Position = Position;
			if (Data.empty()) Base.IsIdentifier.Unset();
			else if (!Base.IsIdentifier)
				Base.IsIdentifier = IdentifierClass(Base.Data);
			Base.FlagRefresh();
			return std::unique_ptr<ReactionT>(Out);
		}
		
		bool Combine(std::unique_ptr<ReactionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Data = Set->Data;
			return true;
		}
	};

	if (Focused == FocusedT::Text)
	{
		struct FinishT : ActionT
		{
			ProtoatomPartT &Base;
			FinishT(ProtoatomPartT &Base) : ActionT("Finish"), Base(Base) { }
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				return Base.Finish({}, Base.Data);
			}
		};
		Core.RegisterAction(make_unique<FinishT>(*this));

		struct TextT : ActionT
		{
			ProtoatomPartT &Base;
			ActionT::TextArgumentT Argument;
			TextT(ProtoatomPartT &Base) : ActionT("Enter text"), Base(Base)
			{ 
				Argument.Regex = std::regex("[a-zA-Z0-9:;\n!@#$%^&*()<>,./\\[\\]{}|\\`~-_'\"=+]");
				Arguments.push_back(&Argument); 
			}

			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				// TODO this should handle arbitrary length text strings
				auto Text = Argument.Data;

				auto NewIsIdentifier = IdentifierClass(Text);
				
				if ((Text == "\n") || (Text == ";"))
				{
					// TODO move to group, have group finish protoatom?
					auto Reactions = new ReactionGroupT;
					auto Finished = Base.Finish({}, Base.Data);
					if (Finished) Reactions->Add(std::move(*Finished));
					auto InputReaction = Base.Core.ReactionHandleInput("Insert statement after");
					//auto InputReaction = Base.Parent->HandleInput(InputT{InputT::MainT::NewStatement, {}});
					if (InputReaction) Reactions->Add(std::move(InputReaction));
					return std::unique_ptr<ReactionT>(Reactions);
				}
				else if (!Base.IsIdentifier || (*Base.IsIdentifier == NewIsIdentifier))
				{
					Assert(Text.length() == 1);
					auto NewData = Base.Data;
					NewData.insert(Base.Position, Text);
					auto NewPosition = Base.Position + 1;
					if (NewPosition == NewData.size()) // Only do auto conversion if appending text
					{
						auto Found = Base.Core.LookUpAtom(NewData);
						if (Found && Found->ReplaceImmediately) 
							return Base.Finish(Found, NewData);
					}
					return std::unique_ptr<ReactionT>(new SetT(Base, NewPosition, NewData));
				}
				else if (Base.IsIdentifier && (*Base.IsIdentifier != NewIsIdentifier))
				{
					Assert(!Base.Data.empty());
					auto Finished = Base.Finish({}, Base.Data);
					Assert(Finished);
					auto ReactionGroup = new ReactionGroupT();
					ReactionGroup->Add(std::move(*Finished));
					ReactionGroup->Add(Base.Core.ReactionHandleInput("Enter text", Text));
					return std::unique_ptr<ReactionT>(ReactionGroup);
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<TextT>(*this));

		struct FocusPreviousT : ActionT
		{
			ProtoatomPartT &Base;
			FocusPreviousT(ProtoatomPartT &Base) : ActionT("Left"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == 0) 
				{
					Base.Parent->FocusPrevious();
				}
				else
				{
					Base.Position -= 1;
					Base.FlagRefresh();
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<FocusPreviousT>(*this));

		struct FocusNextT : ActionT
		{
			ProtoatomPartT &Base;
			FocusNextT(ProtoatomPartT &Base) : ActionT("Right"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) 
				{
					Base.Parent->FocusNext();
				}
				else
				{
					Base.Position += 1;
					Base.FlagRefresh();
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<FocusNextT>(*this));

		struct BackspaceT : ActionT
		{
			ProtoatomPartT &Base;
			BackspaceT(ProtoatomPartT &Base) : ActionT("Backspace"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == 0) return {};
				auto NewData = Base.Data;
				NewData.erase(Base.Position - 1, 1);
				auto NewPosition = Base.Position - 1;
				return std::unique_ptr<ReactionT>(new SetT(Base, NewPosition, NewData));
			}
		};
		Core.RegisterAction(make_unique<BackspaceT>(*this));

		struct DeleteT : ActionT
		{
			ProtoatomPartT &Base;
			DeleteT(ProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) return {};
				auto NewData = Base.Data;
				NewData.erase(Base.Position, 1);
				return std::unique_ptr<ReactionT>(new SetT(Base, Base.Position, NewData));
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		if (!Parent->As<CompositeT>()->HasOnePart())
		{
			struct ExitT : ActionT
			{
				ProtoatomPartT &Base;
				ExitT(ProtoatomPartT &Base) : ActionT("Exit"), Base(Base) {}
				OptionalT<std::unique_ptr<ReactionT>> Apply(void)
				{
					TRACE;
					Base.Core.TextMode = false;
					Base.Focus(FocusDirectionT::Direct);
					return {};
				}
			};
			Core.RegisterAction(make_unique<ExitT>(*this));
		}
	}
	else if (Focused == FocusedT::On)
	{
		struct DeleteT : ActionT
		{
			ProtoatomPartT &Base;
			DeleteT(ProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				return std::unique_ptr<ReactionT>(new SetT(Base, 0, ""));
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		struct EnterT : ActionT
		{
			ProtoatomPartT &Base;
			EnterT(ProtoatomPartT &Base) : ActionT("Enter"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				Base.Core.TextMode = true;
				Base.Focus(FocusDirectionT::Direct);
				return {};
			}
		};
		Core.RegisterAction(make_unique<EnterT>(*this));
	}
	Parent->RegisterActions();
}

void ProtoatomPartT::Defocus(void) 
{
	TRACE;
	Focused = FocusedT::Off;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
	FlagStatusChange();
}

void ProtoatomPartT::AssumeFocus(void)
{
	TRACE;
	Focus(FocusDirectionT::Direct);
}

void ProtoatomPartT::Refresh(void)
{
	TRACE;
	Visual.Start();
	if (Focused == FocusedT::Text)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}

bool ProtoatomPartT::IsEmpty(void) const { TRACE; return Data.empty(); }

OptionalT<std::unique_ptr<ReactionT>> ProtoatomPartT::Finish(OptionalT<AtomTypeT *> Type, std::string Text)
{
	TRACE;
	Assert(Parent->Atom);
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Reactions = new ReactionGroupT;
	if (Type)
	{
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted)
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Finished = Type->Generate(Core);
			Reactions->Add(std::unique_ptr<ReactionT>(new AtomT::SetT(*PartParent()->Atom, Finished)));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : Core.ProtoatomType->Generate(Core);
			Reactions->Add(std::unique_ptr<ReactionT>(new AtomT::SetT(*PartParent()->Atom, Child)));
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
				std::cout << "Parent precedence " << (*WedgeAtom)->PartParent()->GetTypeInfo().Precedence << std::endl;
				WedgeAtom = (*WedgeAtom)->PartParent()->Atom;
				Child = WedgeAtom->Nucleus;
			}
			auto Finished = (*Type)->Generate(Core);
			Assert(Finished);
			Reactions->Add(std::unique_ptr<ReactionT>(new AtomT::SetT(*WedgeAtom, Finished)));
			Reactions->Add(Finished->Set(Child));
		}
	}
	else
	{
		if (Data.empty())
		{
			std::cout << "NOTE: Can't finish as new element if empty." << std::endl; 
			return {};
		}

		if (Lifted) 
		{
			if (Data.empty())
			{
				Assert(Atom);
				std::cout << "Replacing " << this << " with " << Lifted.Nucleus << ", atom " << Atom << std::endl;
				return std::unique_ptr<ReactionT>(new AtomT::SetT(*PartParent()->Atom, Lifted.Nucleus));
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
		
		auto String = Core.StringType->Generate(Core);
		Reactions->Add(String->Set(Text));

		auto Protoatom = Core.ProtoatomType->Generate(Core);

		auto ParentAsComposite = PartParent()->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType))
		{
			Reactions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(String));
		}
		else
		{
			auto Element = Core.ElementType->Generate(Core);
			Reactions->Add(Element->As<CompositeT>()->Parts[1]->Set(String));
			Reactions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(Element));
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Reactions->Add(std::unique_ptr<ReactionT>(new AtomT::SetT(*PartParent()->Atom, Protoatom)));

		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ReactionT>(Reactions);
}

}
