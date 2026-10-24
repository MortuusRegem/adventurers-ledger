QString sourcesLast(const QString &text, bool includeSources = false)
{
    QStringList body, sources;
    bool continuation = false;
    for (auto line : text.split('\n'))
    {
        QString trimmed = line.trimmed();
        if (trimmed == "Sources and version notes")
            continue;
        bool metadata = QRegularExpression("^[A-Za-z]:[\\\\/]").match(trimmed).hasMatch() ||
                        trimmed.startsWith("PDF page:", Qt::CaseInsensitive) ||
                        trimmed.startsWith("Source:", Qt::CaseInsensitive) ||
                        trimmed.startsWith("Reference:", Qt::CaseInsensitive) ||
                        trimmed.startsWith("Older subclass", Qt::CaseInsensitive) ||
                        trimmed.startsWith("Legacy", Qt::CaseInsensitive) ||
                        trimmed.startsWith("Adaptation:", Qt::CaseInsensitive);
        if (metadata || (continuation && !trimmed.isEmpty()))
        {
            sources << trimmed;
            continuation = trimmed.endsWith("initial");
            continue;
        }
        continuation = false;
        QRegularExpression urls("https?://[^\\s]+");
        auto matches = urls.globalMatch(line);
        while (matches.hasNext())
            sources << matches.next().captured();
        line.remove(urls);
        body << line;
    }
    sources.removeDuplicates();
    return body.join('\n').trimmed() +
           (!includeSources || sources.isEmpty()
                ? QString()
                : "\n\nSources and version notes\n" + sources.join('\n'));
}
QString basicChoiceDescription(QString name)
{
    name.remove(QRegularExpression("^(Reviewed: |Level [0-9]+: )"));
    static const QMap<QString, QString> descriptions = {
        {"Strength", "Physical power: lifting, climbing, jumping and Athletics. Usually used for "
                     "melee weapon attacks and damage; some weapons allow Dexterity instead."},
        {"Dexterity",
         "Agility and precision: initiative, Acrobatics, Sleight of Hand, Stealth, ranged attacks "
         "and some Armor Class calculations. Finesse weapons can use it instead of Strength."},
        {"Constitution",
         "Endurance and health. Its modifier affects maximum HP for every character level, "
         "Constitution saves and Concentration saves. It has no associated skill."},
        {"Intelligence",
         "Reasoning and learned knowledge: Arcana, History, Investigation, Nature and Religion. It "
         "is the main spellcasting ability for Wizards and Artificers."},
        {"Wisdom", "Awareness and judgment: Animal Handling, Insight, Medicine, Perception and "
                   "Survival. Clerics, Druids and Rangers use it for spellcasting."},
        {"Charisma", "Presence and influence: Deception, Intimidation, Performance and Persuasion. "
                     "Bards, Paladins, Sorcerers and Warlocks use it for spellcasting."},
        {"Acrobatics",
         "Dexterity skill for balance, tumbling and controlled movement. The DM determines when a "
         "check is needed; it does not automatically replace Athletics for climbing or jumping."},
        {"Animal Handling",
         "Wisdom skill for calming, controlling or understanding an animal's behavior."},
        {"Arcana", "Intelligence skill for recalling magical lore, spells, planes and related "
                   "supernatural knowledge."},
        {"Athletics",
         "Strength skill for demanding physical tasks such as climbing, swimming and jumping."},
        {"Deception",
         "Charisma skill for convincing lies, misleading others and concealing intentions."},
        {"History",
         "Intelligence skill for recalling events, peoples, cultures and historical knowledge."},
        {"Insight", "Wisdom skill for interpreting intentions, emotions and behavior; it is not "
                    "automatic mind reading."},
        {"Intimidation",
         "Charisma skill for influencing others through threats, forceful presence or coercion."},
        {"Investigation", "Intelligence skill for examining clues and reasoning out how things "
                          "work or what happened."},
        {"Medicine", "Wisdom skill for diagnosing illness or injuries and other nonmagical medical "
                     "checks; training does not itself grant healing."},
        {"Nature", "Intelligence skill for recalling lore about animals, plants, terrain, weather "
                   "and the natural world."},
        {"Perception", "Wisdom skill for noticing sights, sounds and other sensory clues. Passive "
                       "Perception normally equals 10 plus your Perception bonus."},
        {"Performance", "Charisma skill for entertaining an audience through music, acting, "
                        "storytelling or similar art."},
        {"Persuasion",
         "Charisma skill for influencing others with honest requests, tact and reasoning."},
        {"Religion",
         "Intelligence skill for recalling deities, rites, holy symbols and religious traditions."},
        {"Sleight of Hand",
         "Dexterity skill for subtle hand movements, concealed objects and pickpocketing."},
        {"Stealth",
         "Dexterity skill for avoiding notice, moving quietly and hiding when the rules allow it."},
        {"Survival", "Wisdom skill for tracking, navigating, finding food and recognizing "
                     "wilderness hazards."}};
    QString result = descriptions.value(name);
    if (abilities.contains(name))
        result += "\n\nThe modifier is floor((score - 10) / 2). Checks use d20 + the modifier; "
                  "applicable training and other bonuses are added separately. Saving throws "
                  "resist effects that call for this ability.";
    else if (!result.isEmpty())
        result += "\n\nRoll d20 + the associated ability modifier. Proficiency adds your "
                  "proficiency bonus; Expertise adds twice that bonus, not both proficiency and "
                  "Expertise again. The app applies supported training to the displayed total.";
    return result;
}
QString masteryDescription(const QString &name)
{
    static const QMap<QString, QString> rules = {
        {"Cleave", "Once per turn after a melee hit, attack a different creature within 5 feet of "
                   "the original target and within reach. A positive ability modifier is omitted "
                   "from that extra attack's damage."},
        {"Graze", "A miss still deals damage equal to the attack's ability modifier, of the "
                  "weapon's damage type. Other damage bonuses do not increase this damage."},
        {"Nick", "Make the Light property's extra attack within the Attack action instead of using "
                 "a Bonus Action. This extra attack remains limited to once per turn."},
        {"Push", "A hit can move a Large or smaller target up to 10 feet directly away from you."},
        {"Sap", "A hit gives the target disadvantage on its next attack roll before your next turn "
                "starts."},
        {"Slow", "A damaging hit can reduce the target's Speed by 10 feet until your next turn "
                 "starts; repeated Slow hits do not increase this reduction."},
        {"Topple", "On a hit, the target makes a Constitution save against 8 + your proficiency "
                   "bonus + the attack's ability modifier. Failure makes it Prone."},
        {"Vex", "A damaging hit gives you advantage on your next attack roll against that creature "
                "before your next turn ends."}};
    return rules.value(name) +
           "\n\nMastery requires the appropriate Weapon Mastery feature and a chosen weapon; "
           "weapon proficiency alone does not unlock it. Apply the effect when its conditions are "
           "met.\nSource: SRD 5.2.1, Equipment";
}
