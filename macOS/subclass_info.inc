QMap<QString, QString> subclassDescriptions()
{
    QMap<QString, QString> descriptions = {
        {"Path of the Berserker",
         "A direct, aggressive warrior who turns Rage into greater damage, retaliatory attacks, "
         "and intimidating presence. Suits a front-line damage dealer."},
        {"Path of the Wild Heart",
         "Channel animal spirits for flexible defensive, movement, and exploration benefits. "
         "Choose animal-themed powers to adapt your raging warrior to different situations."},
        {"Path of the World Tree",
         "Draw on the World Tree to sustain allies, move creatures around the battlefield, and "
         "eventually teleport. Emphasizes protection, reach, and positioning."},
        {"Path of the Zealot",
         "A divinely empowered warrior with extra radiant or necrotic damage and unusual "
         "resilience. Combines relentless melee combat with divine healing and protection."},
        {"College of Dance",
         "An agile, unarmored performer who mixes dance, unarmed strikes, Bardic Inspiration, and "
         "coordinated movement. Helps allies reposition while staying mobile."},
        {"College of Glamour",
         "A Feywild-inspired performer who charms or frightens foes and inspires allies with "
         "protective magic and movement. Emphasizes enchantment and battlefield influence."},
        {"College of Lore",
         "A versatile scholar with additional skill training, Cutting Words to hinder enemies, and "
         "access to more magic. Suits broad utility and magical support."},
        {"College of Valor",
         "A battle-ready bard who combines weapon attacks, armor training, and spellcasting. Uses "
         "inspiration to help companions withstand danger and strike effectively."},
        {"Life Domain", "A dedicated healer whose magic restores more health and sustains allies. "
                        "Choose this domain for strong recovery and dependable party support."},
        {"Light Domain",
         "Wield radiance and fire while using protective flashes of light to disrupt attacks. "
         "Mixes offensive spellcasting with defense against incoming blows."},
        {"Trickery Domain",
         "Use deception, illusions, stealth assistance, and a magical duplicate to misdirect "
         "enemies. Suits infiltration, mobility, and unconventional support."},
        {"War Domain", "A martial cleric who combines weapons with divine combat support. Guided "
                       "strikes and war magic help you and your allies fight effectively."},
        {"Circle of the Land",
         "A nature spellcaster whose terrain-linked magic broadens available spells and supports "
         "endurance. Emphasizes adaptable spellcasting and a deep connection to the environment."},
        {"Circle of the Moon",
         "Specialize in combat Wild Shape with stronger Beast forms and lunar magic. Suits a druid "
         "who wants to fight through transformation."},
        {"Circle of the Sea",
         "Surround yourself with oceanic storm magic that harms or pushes nearby enemies. Later "
         "benefits emphasize storm resistance, flight, and sharing your sea-born power."},
        {"Circle of the Stars",
         "Use starry forms and celestial insight to improve healing, ranged magic, or "
         "concentration. A flexible druid focused on reliable spellcasting and support."},
        {"Battle Master",
         "Learn combat maneuvers fueled by Superiority Dice. Trip, push, protect, or support with "
         "tactical weapon techniques chosen for your fighting style."},
        {"Champion", "A straightforward martial specialist with improved critical hits, athletic "
                     "prowess, and additional fighting expertise. Emphasizes consistent weapon "
                     "combat over resource-heavy tactics."},
        {"Eldritch Knight",
         "Combine Fighter weapon combat with Intelligence-based Wizard spells. Magical defenses "
         "and cantrips supplement your attacks; spell progression is slower than a full caster's."},
        {"Psi Warrior",
         "Use Psionic Energy Dice to protect allies, strengthen attacks, and move creatures or "
         "objects. A weapon fighter with telekinetic defenses and mobility."},
        {"Warrior of Mercy",
         "Channel Focus into healing hands or harmful touches, with later ways to address "
         "conditions. Combines unarmed combat with close-range recovery and support."},
        {"Warrior of Shadow",
         "Use darkness, stealth, and shadow travel to approach and escape enemies. Suits an "
         "infiltrator who controls visibility and fights from unexpected positions."},
        {"Warrior of the Elements",
         "Channel elemental power through extended-reach unarmed attacks, bursts of energy, and "
         "enhanced movement. Emphasizes flexible damage types and battlefield control."},
        {"Warrior of the Open Hand",
         "Master unarmed techniques that push, knock down, or disrupt foes. Combines precise "
         "combat control with self-healing and powerful finishing techniques."},
        {"Oath of Devotion",
         "A sacred defender devoted to honesty and protection. Enhances weapons with holy power "
         "and protects companions against charm and hostile forces."},
        {"Oath of Glory",
         "An athletic hero whose exploits inspire allies. Emphasizes physical achievement, "
         "enhanced mobility, and supporting companions through courageous combat."},
        {"Oath of the Ancients",
         "A nature-aligned protector who defends life and light. Uses restraining magic, "
         "protective auras, and resilient fey-themed powers."},
        {"Oath of Vengeance",
         "A relentless pursuer of dangerous enemies. Mark a chosen foe for focused attacks and use "
         "movement and pursuit magic to keep pressure on it."},
        {"Beast Master",
         "Fight alongside a primal animal companion with land, sea, or sky forms. Coordinate your "
         "attacks and the companion's actions for sustained teamwork."},
        {"Fey Wanderer",
         "Blend ranger combat with Feywild magic, psychic damage, and social influence. Later "
         "powers turn hostile enchantment into opportunities and improve magical mobility."},
        {"Gloom Stalker",
         "A stealthy hunter who excels at ambushes, initiative, and fighting in darkness. Uses "
         "fearsome attacks and concealment to pressure enemies early in a fight."},
        {"Hunter",
         "Choose practical techniques for fighting injured foes or groups, then gain adaptable "
         "defenses. A flexible ranger built around learning prey and exploiting openings."},
        {"Arcane Trickster",
         "Mix Rogue skills with Intelligence-based Wizard magic, including an enhanced Mage Hand. "
         "Spells support stealth, deception, and tactical opportunities."},
        {"Assassin",
         "Specialize in infiltration and decisive opening attacks. Rewards careful preparation, "
         "disguises, and striking before enemies can effectively respond."},
        {"Soulknife",
         "Manifest psychic blades and use Psionic Energy Dice for skills, communication, and "
         "mobility. Suits a subtle operative who needs few visible weapons."},
        {"Thief",
         "An expert at climbing, quick item use, and manipulating equipment under pressure. Later "
         "abilities expand access to magic items and speed up your actions."},
        {"Aberrant Sorcery",
         "Draw on strange, mind-linked magic for telepathy and psionic spellcasting. Emphasizes "
         "psychic influence, subtle casting, and unsettling transformations."},
        {"Clockwork Sorcery",
         "Channel cosmic order to counter chance, protect creatures, and restore balance. "
         "Defensive and restorative magic helps stabilize unpredictable encounters."},
        {"Draconic Sorcery",
         "Manifest a dragon's magical heritage through resilience, elemental affinity, and "
         "eventual flight. Suits an elemental caster who wants greater durability."},
        {"Wild Magic Sorcery",
         "Release unpredictable magical surges and manipulate luck. Emphasizes surprising "
         "outcomes, controlled risk, and increasingly influential wild magic."},
        {"Archfey Patron", "A fey pact built around teleportation, charm, and evasive magic. Misty "
                           "Step and related effects help you reposition and bewilder foes."},
        {"Celestial Patron",
         "A pact with a radiant being grants healing and protective magic alongside Warlock "
         "offense. Suits a caster who wants to support allies without abandoning attacks."},
        {"Fiend Patron",
         "Draw on infernal or abyssal power for aggressive magic, resilience, and rewards when "
         "foes fall. Later powers improve luck, resistance, and punishment."},
        {"Great Old One Patron",
         "An otherworldly pact focused on telepathy, psychic influence, and unnerving mental "
         "magic. Suits subtle manipulation and attacks on an enemy's mind."},
        {"Abjurer", "A defensive Wizard who creates an Arcane Ward and specializes in protective "
                    "magic. Shield yourself and allies while disrupting hostile spellcasting."},
        {"Diviner", "Use magical foresight and Portent to influence crucial rolls. Suits a planner "
                    "who values information, reliability, and control over important outcomes."},
        {"Evoker", "Specialize in destructive evocation and more effective damaging cantrips. "
                   "Shape spell effects around allies and emphasize dependable magical offense."},
        {"Illusionist",
         "An illusion specialist with improved deceptive magic and flexible visual tricks. Shape "
         "what creatures perceive and use increasingly convincing magical creations."}};
    for (auto value : supplementalSubclassCatalog())
    {
        auto entry = value.toObject();
        descriptions[entry["name"].toString()] =
            entry["description"].toString() + "\n\nFeatures: " + entry["features"].toString() +
            "\nSource: " + entry["source"].toString() +
            (entry["legacy"].toBool() ? "\nOlder subclass adapted to the 2024 class: initial "
                                        "features start at class level 3."
                                      : QString()) +
            "\n\nUse Subclass features & choices for supported selections. Conditional actions, "
            "companions and item creation are resolved manually using the source rules.\n" +
            entry["url"].toString();
    }
    for (auto it = descriptions.begin(); it != descriptions.end(); ++it)
        it.value() = sourcesLast(it.value());
    return descriptions;
}
