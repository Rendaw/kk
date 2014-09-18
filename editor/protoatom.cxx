#include "protoatom.h"

#include "../shared/regex.h"

#include "logging.h"
#include "composite.h"
	
static auto AnyClass = std::regex("[a-zA-Z0-9:!@#$%^&*()<>,./\\[\\]{}|\\`~-_'\"=+]");
static auto IdentifierClass = Regex::ParserT<>("^[a-zA-Z0-9_]+$");

namespace Core
{

template <typename TypeT> struct ProtoatomPartTypeT : CompositePartTypeT
{
	using CompositePartTypeT::CompositePartTypeT;

	void Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const { }

	NucleusT *Generate(CoreT &Core)
	{
		TRACE;
		return new TypeT(Core, *this);
	}
};

struct BaseProtoatomPartT : ProtoatomPartT
{
	CompositePartTypeT &TypeInfo;
	
	OptionalT<bool> CouldBeIdentifier;
	enum struct FocusedT
	{
		Off,
		On,
		Text
	} Focused = FocusedT::Off;
	size_t Position = 0;

	BaseProtoatomPartT(CoreT &Core, CompositePartTypeT &TypeInfo) : ProtoatomPartT(Core), TypeInfo(TypeInfo)
	{
		TRACE;
		Visual.SetClass("part");
		Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	}

	Serial::ReadErrorT Deserialize(Serial::ReadObjectT &Object) override
	{
		TRACE;
		Object.Bool("CouldBeIdentifier", [this](bool Value) -> Serial::ReadErrorT { CouldBeIdentifier = Value; return {}; });
		Object.String("Data", [this](std::string &&Value) -> Serial::ReadErrorT { Data = std::move(Value); return {}; });
		Object.UInt("Position", [this](uint64_t Value) -> Serial::ReadErrorT { Position = Value; return {}; }); // Apparently you can assign a uint64_t to a string, with no errors or warnings
		return {};
	}

	void Serialize(Serial::WritePolymorphT &Polymorph) const override
	{
		TRACE;
		/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
		Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
		Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
		if (CouldBeIdentifier) Polymorph.Bool("CouldBeIdentifier", *CouldBeIdentifier);
		Polymorph.String("Data", Data);
		Polymorph.UInt("Position", Position);
	}

	AtomTypeT const &GetTypeInfo(void) const override { return TypeInfo; }
		
	void Focus(FocusDirectionT Direction) override
	{
		TRACE;

		if ((Focused == FocusedT::Off) && (Parent->As<CompositeT>()->Parts.size() == 1)) 
			Core.TextMode = true;

		if (Core.TextMode)
		{
			if (Focused != FocusedT::Text)
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
		}
		else 
		{
			if (Focused != FocusedT::On)
			{
				Visual.SetClass("flag-focused");
				Focused = FocusedT::On;
			}
		}
		FlagRefresh();
		FlagStatusChange();
		NucleusT::Focus(Direction);
		/*if (Parent->Parent && Parent->Parent->As<AtomPartT>())
			Parent->Parent->FlagRefresh(); // Hack for hiding empty protoatoms*/
	}
		
	void RegisterActions(void) override
	{
		TRACE;
		
		if (Focused == FocusedT::Text)
		{
			struct FinishT : ActionT
			{
				BaseProtoatomPartT &Base;
				FinishT(BaseProtoatomPartT &Base) : ActionT("Finish"), Base(Base) { }
				void Apply(void)
				{
					TRACE;
					Base.Finish({}, Base.Data);
				}
			};
			Core.RegisterAction(make_unique<FinishT>(*this));

			struct TextT : ActionT
			{
				BaseProtoatomPartT &Base;
				ActionT::TextArgumentT Argument;
				TextT(BaseProtoatomPartT &Base) : ActionT("Enter text"), Base(Base)
				{ 
					Argument.Regex = AnyClass;
					Arguments.push_back(&Argument); 
				}

				void Apply(void)
				{
					TRACE;
					Base.HandleText(Argument.Data);
				}
			};
			Core.RegisterAction(make_unique<TextT>(*this));

			struct FocusPreviousT : ActionT
			{
				BaseProtoatomPartT &Base;
				FocusPreviousT(BaseProtoatomPartT &Base) : ActionT("Left"), Base(Base) {}
				void Apply(void)
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
				}
			};
			Core.RegisterAction(make_unique<FocusPreviousT>(*this));

			struct FocusNextT : ActionT
			{
				BaseProtoatomPartT &Base;
				FocusNextT(BaseProtoatomPartT &Base) : ActionT("Right"), Base(Base) {}
				void Apply(void)
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
				}
			};
			Core.RegisterAction(make_unique<FocusNextT>(*this));

			struct BackspaceT : ActionT
			{
				BaseProtoatomPartT &Base;
				BackspaceT(BaseProtoatomPartT &Base) : ActionT("Backspace"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					if (Base.Position == 0) return;
					auto NewData = Base.Data;
					NewData.erase(Base.Position - 1, 1);
					auto NewPosition = Base.Position - 1;
					Base.Set(NewPosition, NewData);
				}
			};
			Core.RegisterAction(make_unique<BackspaceT>(*this));

			struct DeleteT : ActionT
			{
				BaseProtoatomPartT &Base;
				DeleteT(BaseProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					if (Base.Position == Base.Data.size()) return;
					auto NewData = Base.Data;
					NewData.erase(Base.Position, 1);
					Base.Set(Base.Position, NewData);
				}
			};
			Core.RegisterAction(make_unique<DeleteT>(*this));

			if (Parent->As<CompositeT>()->Parts.size() > 1)
			{
				struct ExitT : ActionT
				{
					BaseProtoatomPartT &Base;
					ExitT(BaseProtoatomPartT &Base) : ActionT("Exit"), Base(Base) {}
					void Apply(void)
					{
						TRACE;
						Base.Core.TextMode = false;
						Base.Focus(FocusDirectionT::Direct);
					}
				};
				Core.RegisterAction(make_unique<ExitT>(*this));
			}
		}
		else if (Focused == FocusedT::On)
		{
			struct DeleteT : ActionT
			{
				BaseProtoatomPartT &Base;
				DeleteT(BaseProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					Base.Set(0, "");
				}
			};
			Core.RegisterAction(make_unique<DeleteT>(*this));

			struct EnterT : ActionT
			{
				BaseProtoatomPartT &Base;
				EnterT(BaseProtoatomPartT &Base) : ActionT("Enter"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					Base.Core.TextMode = true;
					Base.Focus(FocusDirectionT::Direct);
				}
			};
			Core.RegisterAction(make_unique<EnterT>(*this));
		}
		Parent->RegisterActions();
	}

	void Defocus(void) override
	{
		TRACE;
		Focused = FocusedT::Off;
		Visual.UnsetClass("flag-focused");
		FlagRefresh();
		FlagStatusChange();
	}

	void AssumeFocus(void) override
	{
		TRACE;
		Focus(FocusDirectionT::Direct);
	}

	void Refresh(void) override
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

	void Set(size_t NewPosition, std::string const &NewData)
	{
		TRACE;
		struct SetT : ReactionT
		{
			BaseProtoatomPartT &Base;
			unsigned int Position;
			std::string Data;
			
			SetT(BaseProtoatomPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
			
			void Apply(void) { Base.Set(Position, Data); }
			
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

		Core.AddUndoReaction(make_unique<SetT>(*this, Position, Data));
		Data = NewData;
		Position = NewPosition;
		if (Data.empty()) CouldBeIdentifier.Unset();
		else if (!CouldBeIdentifier) CouldBeIdentifier = IdentifierClass(Data);
		FlagRefresh();
	}

	void HandleText(std::string const &Text) override
	{
		TRACE;
		// TODO this should handle arbitrary length text strings

		auto NewCouldBeIdentifier = IdentifierClass(Text);
		
		if (!CouldBeIdentifier || (*CouldBeIdentifier == NewCouldBeIdentifier))
		{
			Assert(Text.length() == 1);
			auto NewData = Data;
			NewData.insert(Position, Text);
			auto NewPosition = Position + 1;
			if (NewPosition == NewData.size()) // Only do auto conversion if appending text
			{
				auto Found = Core.LookUpAtomType(NewData);
				if (Found && Found->ReplaceImmediately) 
				{
					Finish(Found, NewData);
					return;
				}
			}
			Set(NewPosition, NewData);
		}
		else if (CouldBeIdentifier && (*CouldBeIdentifier != NewCouldBeIdentifier))
		{
			Assert(!Data.empty());
			auto Finished = Finish({}, Data);
			Assert(Finished);
			if (auto Protoatom = Finished->As<ProtoatomT>())
			{
				Protoatom->GetProtoatomPart()->HandleText(Text);
			}
			else if (auto String = Finished->As<StringPartT>())
			{
				String->Set(0, Text);
			}
			else
			{
				std::cout << "Finished was not text capable (for carryover)." << std::endl;
			}
		}
	}

	virtual OptionalT<NucleusT *> Finish(OptionalT<AtomTypeT *> Type, std::string Text) = 0;
};

std::pair<AtomT *, NucleusT *> FindPrecedencePlacement(AtomT *ChildAtom, NucleusT *Child, NucleusT *Replacement);
OptionalT<NucleusT *> TypedFinish(CoreT &Core, bool Bubble, bool Insert, AtomTypeT &Type, AtomT *ParentAtom, OptionalT<NucleusT *> Set);

struct WedgeProtoatomPartT : BaseProtoatomPartT
{
	bool Bubble = false;

	using BaseProtoatomPartT::BaseProtoatomPartT;

	void Defocus(void) override
	{
		if (Data.empty())
		{
			if (Parent)
			{
				auto Lifted = Parent->As<WedgeProtoatomT>()->GetLiftedPart()->Data.Nucleus;
				if (Lifted)
				{
					Core.AddUndoReaction(make_unique<FocusT>(Core, this));
					PartParent()->Atom->Set(Lifted);
				}
			}
		}
		ProtoatomPartT::Defocus();
	}

	OptionalT<NucleusT *> Finish(OptionalT<AtomTypeT *> Type, std::string Text) override
	{
		TRACE;
		Assert(Parent->Atom);
		Type = Type ? Type : Core.LookUpAtomType(Data);

		if (!Type) return {};

		return TypedFinish(
			Core, 
			Bubble, 
			(bool)dynamic_cast<InsertProtoatomTypeT const *>(&PartParent()->GetTypeInfo()), 
			**Type, 
			PartParent()->Atom, 
			PartParent()->As<WedgeProtoatomT>()->GetLiftedPart()->Data.Nucleus);
	}
};

struct SoloProtoatomPartT : BaseProtoatomPartT
{
	using BaseProtoatomPartT::BaseProtoatomPartT;
	
	OptionalT<NucleusT *> Finish(OptionalT<AtomTypeT *> Type, std::string Text) override
	{
		TRACE;
		Assert(Parent->Atom);
		Type = Type ? Type : Core.LookUpAtomType(Data);
		if (Type)
		{
			return TypedFinish(
				Core, 
				false, 
				false, 
				**Type, 
				PartParent()->Atom, 
				{});
		}
		else
		{
			if (Data.empty())
			{
				std::cout << "NOTE: Can't finish as new element if empty." << std::endl; 
				return {};
			}

			auto String = Core.StringType->Generate(Core);
			GetStringPart(String)->Set(0, Text);

			auto Protoatom = Core.AppendProtoatomType->Generate(Core);
			Protoatom->As<ProtoatomT>()->GetProtoatomPart()->As<WedgeProtoatomPartT>()->Bubble = true;

			auto ParentAsComposite = PartParent()->As<CompositeT>();
			if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType))
			{
				Protoatom->As<WedgeProtoatomT>()->SetLifted(String);
			}
			else
			{
				auto Element = Core.ElementType->Generate(Core);
				GetElementRightPart(Element)->Data.Set(String);
				Protoatom->As<WedgeProtoatomT>()->SetLifted(Element);
			}

			PartParent()->Atom->Set(Protoatom);

			std::cout << Core.Dump() << std::endl; // DEBUG
			std::cout << "Standard no-type finish." << std::endl;
			Protoatom->Focus(FocusDirectionT::Direct);
			return Protoatom;
		}
	}
};

std::pair<AtomT *, NucleusT *> FindPrecedencePlacement(AtomT *ChildAtom, NucleusT *Child, NucleusT *Replacement)
{
	std::cout << "Finding of placement of " << (*ChildAtom)->PartParent()->GetTypeInfo().Tag << " vs " << Replacement->GetTypeInfo().Tag << std::endl;
	while ((*ChildAtom)->PartParent() && !IsPrecedent(*ChildAtom, Replacement))
	{
		ChildAtom = (*ChildAtom)->PartParent()->Atom;
		Child = ChildAtom->Nucleus;
		std::cout << "not precedent, moving up: " << (*ChildAtom)->PartParent()->GetTypeInfo().Tag << std::endl;
	}
	return {ChildAtom, Child};
}

OptionalT<NucleusT *> TypedFinish(CoreT &Core, bool Bubble, bool Insert, AtomTypeT &Type, AtomT *ParentAtom, OptionalT<NucleusT *> Set)
{
	OptionalT<size_t> CarryoverPlaces;
	OptionalT<OperatorDirectionT> Direction; 
	if (Insert)
	{
		Assert(!Bubble);
		Direction = OperatorDirectionT::Right;
		CarryoverPlaces = 1;
		Bubble = false;
	}
	else
	{
		Direction = OperatorDirectionT::Left;
		CarryoverPlaces = 0;
	}

	HoldT Finished(Core, Type.Generate(Core));
	Assert(Finished);

	auto SetOperand = Finished->As<CompositeT>()->GetOperand(*Direction, 0);
	NucleusT *Carryover = nullptr;
	{
		auto CarryoverOperand = Finished->As<CompositeT>()->GetAnyOperand(OperatorDirectionT::Right, *CarryoverPlaces);
		Assert(CarryoverOperand);
		if (CarryoverOperand) Carryover = *CarryoverOperand;
	}

	auto WedgeBottom = ParentAtom;
	NucleusT *WedgeTop = nullptr;

	if (!SetOperand) 
	{
		if (Set) return {}; // Invalid type
	}
	else
	{
		if (Set)
		{
			WedgeTop = *Set;	
			WedgeBottom->Set(WedgeTop); // Drop lifted before bubbling
		}

		if (Bubble)
		{
			auto Placement = FindPrecedencePlacement(WedgeBottom, WedgeTop, Finished.Nucleus);
			WedgeBottom = Placement.first;
			WedgeTop = Placement.second;
		}

		if (WedgeTop)
			SetOperand->Set(WedgeTop);
	}

	if (Carryover)
	{
		WedgeBottom->Set(Finished.Nucleus);
	}
	else
	{
		Carryover = Core.AppendProtoatomType->Generate(Core);
		Carryover->As<ProtoatomT>()->GetProtoatomPart()->As<WedgeProtoatomPartT>()->Bubble = true;
		Carryover->As<WedgeProtoatomT>()->GetLiftedPart()->Data.Set(Finished.Nucleus);
		WedgeBottom->Set(Carryover);
	}

	Assert(Carryover);
	return Carryover;
}

SoloProtoatomTypeT::SoloProtoatomTypeT(void)
{
	TRACE;
	Tag = "SoloProtoatom";
	auto ProtoatomPart = make_unique<ProtoatomPartTypeT<SoloProtoatomPartT>>(*this);
	ProtoatomPart->FocusDefault = true;
	Parts.push_back(std::move(ProtoatomPart));
}
	
NucleusT *SoloProtoatomTypeT::Generate(CoreT &Core)
	{ TRACE; return GenerateComposite<SoloProtoatomT>(*this, Core); }
	
InsertProtoatomTypeT::InsertProtoatomTypeT(void)
{
	TRACE;
	Tag = "InsertProtoatom";
	Precedence = 800;
	LeftAssociative = false;
	auto ProtoatomPart = make_unique<ProtoatomPartTypeT<WedgeProtoatomPartT>>(*this);
	ProtoatomPart->FocusDefault = true;
	Parts.push_back(std::move(ProtoatomPart));
	auto AtomPart = new AtomPartTypeT(*this);
	AtomPart->StartEmpty = true;
	Parts.push_back(std::unique_ptr<CompositePartTypeT>(AtomPart));
}

NucleusT *InsertProtoatomTypeT::Generate(CoreT &Core)
	{ TRACE; return GenerateComposite<WedgeProtoatomT>(*this, Core); }

AppendProtoatomTypeT::AppendProtoatomTypeT(void)
{
	TRACE;
	Tag = "AppendProtoatom";
	Precedence = 800;
	LeftAssociative = true;
	auto AtomPart = new AtomPartTypeT(*this);
	AtomPart->StartEmpty = true;
	Parts.push_back(std::unique_ptr<CompositePartTypeT>(AtomPart));
	auto ProtoatomPart = make_unique<ProtoatomPartTypeT<WedgeProtoatomPartT>>(*this);
	ProtoatomPart->FocusDefault = true;
	Parts.push_back(std::move(ProtoatomPart));
}

NucleusT *AppendProtoatomTypeT::Generate(CoreT &Core)
	{ TRACE; return GenerateComposite<WedgeProtoatomT>(*this, Core); }

bool SoloProtoatomT::IsEmpty(void)
{
	return Parts[0]->As<SoloProtoatomPartT>()->Data.empty();
}

ProtoatomPartT *SoloProtoatomT::GetProtoatomPart(void)
{
	return *Parts[0]->As<ProtoatomPartT>();
}

ProtoatomPartT *WedgeProtoatomT::GetProtoatomPart(void)
{
	return *(dynamic_cast<InsertProtoatomTypeT *>(&TypeInfo) ? Parts[0] : Parts[1])->As<ProtoatomPartT>();
}

void WedgeProtoatomT::SetLifted(NucleusT *Lifted)
{
	(dynamic_cast<InsertProtoatomTypeT *>(&TypeInfo) ? Parts[1] : Parts[0])->As<AtomPartT>()->Data.Set(Lifted);
}

AtomPartT *WedgeProtoatomT::GetLiftedPart(void)
{
	return *(dynamic_cast<InsertProtoatomTypeT *>(&TypeInfo) ? Parts[1] : Parts[0])->As<AtomPartT>();
}
	
}

