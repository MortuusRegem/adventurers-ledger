bool selfTestAdvancement()
{
    auto original = character;
    bool previousDirty = dirty;
    auto fail = [&](QString reason)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Advancement: " + reason).toUtf8());
        character = original;
        loadWidgets();
        dirty = previousDirty;
        return false;
    };
    if (advancementCatalog.size() != 13)
        return fail("class catalog");
    auto pendingCount = [&](QString type)
    {
        int count = 0;
        for (auto v : array("advancementTasks"))
            if (v.toObject()["type"] == type)
                count += v.toObject()["count"].toInt();
        return count;
    };
    for (auto cls : classes)
        for (int n = 1; n <= 20; ++n)
        {
            auto step = classStep(cls, n);
            if (step["level"].toInt() != n)
                return fail("missing class level");
        }
    for (auto cls : classes)
        for (int target : QList<int>{2, 4, 6, 10, 19, 20})
        {
            character = blank();
            character["class"] = cls;
            character["primaryLevel"] = target - 1;
            character["level"] = target - 1;
            character["subclass"] = subclasses.value(cls).first();
            for (auto ability : abilities)
                character[ability] = 15;
            character["slotsUsed"] = QJsonArray{1, 0, 0, 0, 0, 0, 0, 0, 0};
            character["pactUsed"] = 1;
            character["pactMax"] = cls == "Warlock" ? classStat(cls, "Pact slots", target - 1) : 0;
            character["pactLevel"] =
                cls == "Warlock" ? classStat(cls, "Pact level", target - 1) : 1;
            character["classResourcesSpent"] = QJsonObject{{"Fighter:Second Wind", 1}};
            loadWidgets();
            QString error;
            int oldHp = number("hpMax"), oldCurrent = number("hpCurrent"), oldMax = maximumHp();
            if (!applyLevelGain(cls, hitDie(cls) / 2 + 1, error))
                return fail(cls + QString::number(target) + ": " + error);
            if (number("hpMax") != oldHp + hitDie(cls) / 2 + 3 || number("level") != target ||
                classLevel(cls) != target)
                return fail("HP or class level");
            if (number("hpCurrent") != oldCurrent + maximumHp() - oldMax)
                return fail("current HP must preserve missing HP");
            bool feat = QList<int>{4, 8, 12, 16, 19}.contains(target) ||
                        (cls == "Fighter" && QList<int>{6, 14}.contains(target)) ||
                        (cls == "Rogue" && target == 10);
            if (pendingCount("feat") != (feat ? 1 : 0))
                return fail("feat schedule: " + cls + QString::number(target));
            if (character["classResourcesSpent"].toObject()["Fighter:Second Wind"].toInt() != 1)
                return fail("spent class resources reset");
            if (classSpellLimit(cls, target) > 0 && cls != "Warlock" &&
                array("slotsUsed").at(0).toInt() != 1)
                return fail("spent slots reset");
            if (cls == "Warlock" && number("pactUsed") != 1)
                return fail("spent pact slots reset");
        }
    character = blank();
    character["Intelligence"] = 12;
    loadWidgets();
    auto before = character;
    QString error;
    if (applyLevelGain("Wizard", 4, error) || character != before)
        return fail("multiclass prerequisites");
    character["Intelligence"] = 13;
    character["primaryLevel"] = 3;
    character["level"] = 3;
    loadWidgets();
    if (!applyLevelGain("Wizard", 4, error) || classLevel("Wizard") != 1 ||
        classLevel("Fighter") != 3 || pendingCount("feat") != 0 ||
        array("saveProfs").at(3).toBool())
        return fail("multiclass class-level awards");
    if (pendingCount("spell") != 6 || pendingCount("cantrip") != 3)
        return fail("Wizard first level spells");
    auto pending = character;
    loadWidgets();
    if (character["advancementTasks"] != pending["advancementTasks"])
        return fail("pending choices lost");
    if (!lockAbilities() || character["abilitiesLocked"].toBool())
        return fail("saving pending draft locked abilities");
    if (applyLevelGain("Fighter", 6, error))
        return fail("allowed another level with pending choices");
    character = blank();
    character["class"] = "Rogue";
    character["primaryLevel"] = 5;
    character["level"] = 5;
    character["subclass"] = "Thief";
    loadWidgets();
    if (!applyLevelGain("Rogue", 5, error) || pendingCount("expertise") != 2)
        return fail("Rogue expertise");
    character = blank();
    character["class"] = "Ranger";
    character["subclass"] = "Hunter";
    loadWidgets();
    if (!applyLevelGain("Ranger", 6, error) || pendingCount("expertise") != 1 ||
        pendingCount("language") != 2 || pendingCount("style") != 1)
        return fail("Ranger choices");
    character = blank();
    character["class"] = "Bard";
    character["manageClassProgression"] = true;
    character["primaryLevel"] = 2;
    character["level"] = 2;
    loadWidgets();
    int raw = ability(skillAbilities[3]);
    if (skillBonus(3) != raw + 1)
        return fail("Jack of All Trades");
    character = blank();
    character["class"] = "Monk";
    character["manageClassProgression"] = true;
    character["primaryLevel"] = 14;
    character["level"] = 14;
    loadWidgets();
    if (effectTraining({"save:*", "save:Intelligence"}, 0) != 1 || effectiveSpeed() != 55)
        return fail("Monk features");
    character = blank();
    character["class"] = "Paladin";
    character["manageClassProgression"] = true;
    character["primaryLevel"] = 6;
    character["level"] = 6;
    loadWidgets();
    if (effectBonus({"save:*"}) != 1)
        return fail("Paladin aura minimum");
    character["classTraitsActive"] = false;
    if (effectBonus({"save:*"}) != 0)
        return fail("inactive aura");
    character = blank();
    character["class"] = "Fighter";
    character["primaryLevel"] = 3;
    character["level"] = 3;
    character["subclass"] = "Champion";
    loadWidgets();
    if (!applyLevelGain("Fighter", 6, error))
        return fail(error);
    bool picker = false;
    int featTask = -1;
    auto tasks = array("advancementTasks");
    for (int i = 0; i < tasks.size(); ++i)
        if (tasks[i].toObject()["type"] == "feat")
            featTask = i;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *list = dialog->findChild<QListWidget *>();
            if (!list)
            {
                dialog->reject();
                return;
            }
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text() == "Alert")
                {
                    list->item(i)->setCheckState(Qt::Checked);
                    picker = true;
                }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    if (!resolveAdvanceTask(featTask) || !picker || !hasFeat("Alert") ||
        !array("advancementTasks").at(featTask).toObject()["done"].toBool())
        return fail("earned feat chooser");
    character["classResourcesSpent"] =
        QJsonObject{{"Fighter:Second Wind", 2}, {"Fighter:Action Surge", 1}};
    restoreClassResources(false);
    if (character["classResourcesSpent"].toObject()["Fighter:Second Wind"].toInt() != 1 ||
        character["classResourcesSpent"].toObject()["Fighter:Action Surge"].toInt() != 0)
        return fail("short rest resources");
    restoreClassResources(true);
    if (character["classResourcesSpent"].toObject()["Fighter:Second Wind"].toInt() != 0)
        return fail("long rest resources");
    showAdvancement();
    advancementPage->grab().save(QCoreApplication::applicationDirPath() +
                                 "/preview-advancement.png");
    character = original;
    loadWidgets();
    dirty = previousDirty;
    return true;
}
int classLevel(const QString &name) const
{
    for (auto v : classRoster())
        if (v.toObject()["name"].toString() == name)
            return v.toObject()["level"].toInt();
    return 0;
}
QJsonObject classStep(const QString &name, int level) const
{
    if (level < 1 || level > 20)
        return {};
    return advancementCatalog[name].toObject()["levels"].toArray().at(level - 1).toObject();
}
int classStat(const QString &name, const QString &key, int level = -1) const
{
    return classStep(name, level < 0 ? classLevel(name) : level)["stats"]
        .toObject()[key]
        .toString()
        .toInt();
}
QString castingFor(const QString &cls) const
{
    if (QStringList{"Bard", "Paladin", "Sorcerer", "Warlock"}.contains(cls))
        return "Charisma";
    if (cls == "Monk" && isThirdCaster(cls))
        return "Wisdom";
    if (QStringList{"Cleric", "Druid", "Ranger"}.contains(cls))
        return "Wisdom";
    return "Intelligence";
}
QStringList classSkills(const QString &cls) const
{
    return startingSkills(cls);
}
QString multiclassProblem(const QString &cls) const
{
    if (classLevel(cls) > 0)
        return {};
    static const QMap<QString, QStringList> required{{"Artificer", {"Intelligence"}},
                                                     {"Barbarian", {"Strength"}},
                                                     {"Bard", {"Charisma"}},
                                                     {"Cleric", {"Wisdom"}},
                                                     {"Druid", {"Wisdom"}},
                                                     {"Fighter", {"Strength|Dexterity"}},
                                                     {"Monk", {"Dexterity", "Wisdom"}},
                                                     {"Paladin", {"Strength", "Charisma"}},
                                                     {"Ranger", {"Dexterity", "Wisdom"}},
                                                     {"Rogue", {"Dexterity"}},
                                                     {"Sorcerer", {"Charisma"}},
                                                     {"Warlock", {"Charisma"}},
                                                     {"Wizard", {"Intelligence"}}};
    QStringList check{cls};
    for (auto v : classRoster())
        check << v.toObject()["name"].toString();
    for (auto name : check)
        for (auto requirement : required.value(name))
        {
            bool met = false;
            for (auto a : requirement.split('|'))
                if (effectiveScore(abilities.indexOf(a)) >= 13)
                    met = true;
            if (!met)
                return name + " requires " + QString(requirement).replace('|', " or ") +
                       " 13 to multiclass.";
        }
    return {};
}
QJsonArray classEffects() const
{
    QJsonArray out;
    if (!character["manageClassProgression"].toBool())
        return out;
    auto add =
        [&](QString target, int value, QString op = "bonus", QString scale = "flat", int cap = 30)
    {
        QJsonObject e{{"target", target}, {"value", value}, {"op", op},
                      {"scale", scale},   {"cap", cap},     {"source", "Class progression"}};
        out.append(e);
    };
    bool armor = !wornCatalogArmor().isEmpty() || character["wearingArmor"].toBool();
    bool heavy = wornCatalogArmor()["armorCategory"] == "Heavy Armor";
    bool shield = !wornCatalogArmor(true).isEmpty();
    if (classLevel("Barbarian") >= 5 && !heavy)
        add("speed", 10);
    if (classLevel("Barbarian") >= 2 && character["classTraitsActive"].toBool(true))
        add("save:Dexterity", 0, "advantage");
    if (classLevel("Barbarian") >= 7 && character["classTraitsActive"].toBool(true))
        add("initiative", 0, "advantage");
    if (classLevel("Monk") >= 2 && !armor && !shield)
        add("speed", classStat("Monk", "Unarmored Movement"));
    if (classLevel("Ranger") >= 6 && !heavy)
        add("speed", 10);
    if (classLevel("Monk") >= 14)
        add("save:*", 1, "proficiency");
    if (classLevel("Rogue") >= 15)
    {
        add("save:Wisdom", 1, "proficiency");
        add("save:Charisma", 1, "proficiency");
    }
    if (classLevel("Barbarian") == 20)
    {
        add("score:Strength", 4, "bonus", "flat", 25);
        add("score:Constitution", 4, "bonus", "flat", 25);
    }
    if (classLevel("Monk") == 20)
    {
        add("score:Dexterity", 4, "bonus", "flat", 25);
        add("score:Wisdom", 4, "bonus", "flat", 25);
    }
    if (classLevel("Paladin") >= 6 && character["classTraitsActive"].toBool(true))
    {
        QJsonObject e{{"target", "save:*"}, {"value", 1},
                      {"op", "bonus"},      {"scale", "Charisma"},
                      {"minimumValue", 1},  {"source", "Aura of Protection (self)"}};
        out.append(e);
    }
    auto choices = character["classChoices"].toObject();
    for (auto cls : QStringList{"Cleric", "Druid"})
    {
        QString key = cls + ":order";
        QString chosen = choices[key].toString();
        if (chosen == "Thaumaturge" || chosen == "Magician")
            for (auto skill : QStringList{"Arcana", cls == "Cleric" ? "Religion" : "Nature"})
                out.append(QJsonObject{{"target", "skill:" + skill},
                                       {"value", 1},
                                       {"op", "bonus"},
                                       {"scale", "Wisdom"},
                                       {"minimumValue", 1},
                                       {"source", chosen}});
    }
    if (character["classOptions"].toObject()["Warlock"].toArray().contains("Eldritch Mind"))
        add("concentration", 0, "advantage");
    return out;
}
int classJackBonus(int skill) const
{
    if (!character["manageClassProgression"].toBool() || classLevel("Bard") < 2)
        return 0;
    return effectTraining({"skill:*", "skill:" + skillNames[skill]},
                          array("skillProfs").at(skill).toInt()) == 0
               ? pb() / 2
               : 0;
}
bool classWeaponTrained(const QJsonObject &weapon) const
{
    if (!character["manageClassProgression"].toBool())
        return false;
    QJsonObject ref;
    for (auto v : catalog["weapons"].toArray())
        if (v.toObject()["name"] == weapon["name"])
            ref = v.toObject();
    QString category = ref["category"].toString(), properties = ref["properties"].toString();
    if (category.startsWith("Simple"))
        return true;
    if (!category.startsWith("Martial"))
        return false;
    for (auto cls : classes)
        if (classLevel(cls) >= 3 && subclassDefinition(cls)["martial"].toBool())
            return true;
    for (auto cls : QStringList{"Barbarian", "Fighter", "Paladin", "Ranger"})
        if (classLevel(cls) > 0)
            return true;
    if (text("class") == "Rogue" &&
        (properties.contains("Finesse") || properties.contains("Light")))
        return true;
    if (text("class") == "Monk" && category.contains("Melee") && properties.contains("Light"))
        return true;
    auto choices = character["classChoices"].toObject();
    return choices["Cleric:order"] == "Protector" || choices["Druid:order"] == "Warden";
}
int classUnarmoredAc() const
{
    if (!character["manageClassProgression"].toBool() || character["wearingArmor"].toBool())
        return 0;
    int subclassAc = 0;
    if (classLevel("Sorcerer") >= 3 && subclassFor("Sorcerer") == "Draconic Sorcery")
        subclassAc = 10 + ability(1) + ability(5);
    if (classLevel("Bard") >= 3 && subclassFor("Bard") == "College of Dance" &&
        wornCatalogArmor(true).isEmpty())
        subclassAc = qMax(subclassAc, 10 + ability(1) + ability(5));
    // Only the first class to grant Unarmored Defense is eligible when multiclassing.
    for (auto v : classRoster())
    {
        QString cls = v.toObject()["name"].toString();
        if (cls == "Barbarian")
            return qMax(subclassAc, 10 + ability(1) + ability(2));
        if (cls == "Monk")
            return qMax(subclassAc,
                        wornCatalogArmor(true).isEmpty() ? 10 + ability(1) + ability(4) : 0);
    }
    return subclassAc;
}
QStringList pendingAdvanceNames() const
{
    QStringList out;
    for (auto v : array("advancementTasks"))
        if (!v.toObject()["done"].toBool())
            out << v.toObject()["title"].toString();
    return out;
}
void addAdvanceTask(QJsonArray &tasks, QString cls, int level, QString type, QString title,
                    int count = 1, QJsonObject extra = {})
{
    extra["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    extra["class"] = cls;
    extra["level"] = level;
    extra["type"] = type;
    extra["title"] = title;
    extra["count"] = count;
    extra["done"] = false;
    tasks.append(extra);
}
int classSpellLimit(const QString &cls, int n) const
{
    if (isThirdCaster(cls))
        return n < 3 ? 0 : n < 7 ? 1 : n < 13 ? 2 : n < 19 ? 3 : 4;
    int highest = 0;
    auto stats = classStep(cls, n)["stats"].toObject();
    for (int i = 1; i <= 9; ++i)
        if (stats["Slot " + QString::number(i)].toString().toInt() > 0)
            highest = i;
    if (cls == "Warlock")
        highest = stats["Pact level"].toString().toInt();
    return highest;
}
void buildAdvanceTasks(const QString &cls, int n)
{
    auto tasks = array("advancementTasks");
    QString feature = classStep(cls, n)["features"].toString();
    if (feature.contains("Ability Score Improvement") || feature.contains("Epic Boon"))
        addAdvanceTask(tasks, cls, n, "feat",
                       feature.contains("Epic Boon")
                           ? "Choose an Epic Boon or another eligible feat"
                           : "Choose a feat or Ability Score Improvement");
    if (feature.contains("Fighting Style"))
        addAdvanceTask(tasks, cls, n, "style", "Choose a Fighting Style");
    QString subclass;
    for (auto v : classRoster())
        if (v.toObject()["name"].toString() == cls)
            subclass = v.toObject()["subclass"].toString();
    if (n >= 3 && subclass.isEmpty())
        addAdvanceTask(tasks, cls, n, "subclass", "Choose " + cls + " subclass");
    int expertise = cls == "Bard" && (n == 2 || n == 9)    ? 2
                    : cls == "Rogue" && (n == 1 || n == 6) ? 2
                    : cls == "Ranger" && n == 2            ? 1
                    : cls == "Ranger" && n == 9            ? 2
                    : cls == "Wizard" && n == 2            ? 1
                                                           : 0;
    if (expertise)
        addAdvanceTask(tasks, cls, n, "expertise",
                       "Choose " + QString::number(expertise) + " skill expertise", expertise);
    if ((n == 1 && QStringList{"Bard", "Ranger", "Rogue"}.contains(cls)) ||
        (cls == "Barbarian" && n == 3))
        addAdvanceTask(tasks, cls, n, "skill", "Choose an additional skill proficiency");
    if ((cls == "Ranger" && n == 2) || (cls == "Rogue" && n == 1))
        addAdvanceTask(tasks, cls, n, "language", "Choose additional languages",
                       cls == "Ranger" ? 2 : 1);
    if (n == 1 && (cls == "Cleric" || cls == "Druid"))
        addAdvanceTask(tasks, cls, n, "order",
                       "Choose " + QString(cls == "Cleric" ? "Divine" : "Primal") + " Order");
    if (isThirdCaster(cls) && n > 3)
    {
        int extra = thirdCasterPrepared(n) - thirdCasterPrepared(n - 1);
        if (extra > 0)
            addAdvanceTask(tasks, cls, n, "spell", "Choose additional subclass spells", extra,
                           QJsonObject{{"spellClass", thirdCasterList(cls)},
                                       {"maxSpellLevel", classSpellLimit(cls, n)}});
        if (n == 10)
            addAdvanceTask(tasks, cls, n, "cantrip", "Choose an additional subclass cantrip", 1,
                           QJsonObject{{"spellClass", thirdCasterList(cls)}});
    }
    int prepared = classStat(cls, "Prepared spells", n) - classStat(cls, "Prepared spells", n - 1);
    int cantrips = classStat(cls, "Cantrips", n) - classStat(cls, "Cantrips", n - 1);
    if (cantrips > 0)
        addAdvanceTask(tasks, cls, n, "cantrip",
                       "Learn " + QString::number(cantrips) + " cantrip(s)", cantrips);
    if (cls == "Wizard")
        addAdvanceTask(tasks, cls, n, "spell",
                       "Add " + QString::number(n == 1 ? 6 : 2) + " Wizard spells to spellbook",
                       n == 1 ? 6 : 2);
    else if (prepared > 0)
        addAdvanceTask(tasks, cls, n, "spell",
                       "Choose " + QString::number(prepared) + " additional prepared spell(s)",
                       prepared);
    if (classSpellLimit(cls, n) > 0)
        addAdvanceTask(tasks, cls, n, "review",
                       "Review prepared spells and optional spell replacements");
    if (cls == "Sorcerer" && (n == 2 || n == 10 || n == 17))
        addAdvanceTask(tasks, cls, n, "options", "Choose two Metamagic options", 2);
    int invocations = cls == "Warlock"
                          ? classStat(cls, "Invocations", n) - classStat(cls, "Invocations", n - 1)
                          : 0;
    if (invocations > 0)
        addAdvanceTask(tasks, cls, n, "options",
                       "Choose " + QString::number(invocations) + " invocation(s)", invocations);
    if (cls == "Warlock" && QList<int>{11, 13, 15, 17}.contains(n))
        addAdvanceTask(tasks, cls, n, "arcanum", "Choose Mystic Arcanum", 1,
                       QJsonObject{{"spellLevel", (n + 1) / 2}});
    if (cls == "Fighter" && subclass == "Champion" && n == 7)
        addAdvanceTask(tasks, cls, n, "style", "Champion: additional Fighting Style");
    if (cls == "Bard" && subclass == "College of Lore" && n == 3)
        addAdvanceTask(tasks, cls, n, "skill", "Bonus Proficiencies: choose three skills", 3,
                       QJsonObject{{"anySkill", true}});
    if ((cls == "Fighter" && subclass == "Eldritch Knight") ||
        (cls == "Rogue" && subclass == "Arcane Trickster"))
        addAdvanceTask(tasks, cls, n, "review",
                       "Review subclass spellcasting and learn spells from its progression table");
    queueSubclassReview(tasks, cls, n);
    if (feature != "—")
        addAdvanceTask(tasks, cls, n, "review",
                       "Review new class and subclass features: " + feature);
    character["advancementTasks"] = tasks;
}
void addClassSpell(const QString &name, const QString &cls, bool prepared = true)
{
    QString canonical = name;
    auto normalized = [](QString s) { return s.replace(QChar(0x2019), '\'').toLower(); };
    for (auto v : catalog["spells"].toArray())
        if (normalized(v.toObject()["name"].toString()) == normalized(name))
            canonical = v.toObject()["name"].toString();
    if (findSpell(canonical).isEmpty())
        return;
    auto spells = array("spells");
    for (int i = 0; i < spells.size(); ++i)
        if (spells[i].toObject()["name"].toString() == canonical)
        {
            auto entry = spells[i].toObject();
            QString learnedFrom = entry["learnedFrom"].toString();
            if (!learnedFrom.isEmpty() && learnedFrom != cls)
                continue;
            if (learnedFrom.isEmpty() && !entry["castingAbility"].toString().isEmpty() &&
                entry["castingAbility"].toString() != castingFor(cls))
                continue;
            entry["learnedFrom"] = cls;
            entry["castingAbility"] = castingFor(cls);
            if (prepared)
                entry["prepared"] = true;
            spells[i] = entry;
            character["spells"] = spells;
            return;
        }
    spells.append(QJsonObject{{"name", canonical},
                              {"prepared", prepared},
                              {"castingAbility", castingFor(cls)},
                              {"learnedFrom", cls}});
    character["spells"] = spells;
}
bool applyLevelGain(const QString &cls, int hitDieResult, QString &error)
{
    if (!classes.contains(cls) || number("level", 1) >= 20)
    {
        error = "A valid class and a total level below 20 are required.";
        return false;
    }
    if (!pendingAdvanceNames().isEmpty())
    {
        error = "Finish the pending advancement choices first.";
        return false;
    }
    if (!creationProblem().isEmpty())
    {
        error = "Complete the required starting proficiencies before leveling up.";
        return false;
    }
    error = abilityBudgetProblem();
    if (!error.isEmpty())
        return false;
    error = multiclassProblem(cls);
    if (!error.isEmpty())
        return false;
    if (hitDieResult < 1 || hitDieResult > hitDie(cls))
    {
        error = "Invalid Hit Die result.";
        return false;
    }
    auto before = character;
    int previousMaximum = maximumHp(), previousCurrent = number("hpCurrent");
    if (character["levelUpDraft"].toBool())
        character["hpMax"] =
            number("hpMax") +
            number("level", 1) * (modifier(number("Constitution")) - number("levelUpConModifier"));
    int n = classLevel(cls) + 1;
    if (cls == text("class"))
        character["primaryLevel"] = n;
    else
    {
        auto roster = array("multiclasses");
        bool found = false;
        for (int i = 0; i < roster.size(); ++i)
        {
            auto c = roster[i].toObject();
            if (c["name"].toString() == cls)
            {
                c["level"] = n;
                roster[i] = c;
                found = true;
                break;
            }
        }
        if (!found)
            roster.append(QJsonObject{{"name", cls}, {"level", 1}, {"subclass", ""}});
        character["multiclasses"] = roster;
    }
    character["level"] = number("level", 1) + 1;
    character["hpMax"] = number("hpMax") + qMax(1, hitDieResult + modifier(number("Constitution")));
    character["levelUpConModifier"] = modifier(number("Constitution"));
    character["levelUpDraft"] = true;
    character["abilitiesLocked"] = false;
    character["manageClassProgression"] = true;
    buildAdvanceTasks(cls, n);
    auto languages = text("languages").split(", ", Qt::SkipEmptyParts);
    if (n == 1 && (cls == "Rogue" || cls == "Druid"))
    {
        QString language = cls == "Rogue" ? "Thieves’ Cant" : "Druidic";
        if (!languages.contains(language))
            languages << language;
        character["languages"] = languages.join(", ");
    }
    if (cls == "Ranger")
        addClassSpell("Hunter's Mark", cls);
    if (cls == "Paladin" && n >= 2)
        addClassSpell("Divine Smite", cls);
    if (cls == "Paladin" && n >= 5)
        addClassSpell("Find Steed", cls);
    if (cls == "Warlock" && n >= 9)
        addClassSpell("Contact Other Plane", cls);
    if (cls == "Bard" && n == 20)
    {
        addClassSpell("Power Word Heal", cls);
        addClassSpell("Power Word Kill", cls);
    }
    auto oldUsed = array("slotsUsed");
    int pactSpent = number("pactUsed");
    loadWidgets();
    configureSlots(false);
    QJsonArray retained;
    for (int i = 0; i < 9; ++i)
        retained.append(qMin(oldUsed.at(i).toInt(), array("slotsMax").at(i).toInt()));
    character["slotsUsed"] = retained;
    character["pactUsed"] = qMin(pactSpent, number("pactMax"));
    if (previousCurrent > 0)
        character["hpCurrent"] =
            qBound(0, previousCurrent + maximumHp() - previousMaximum, maximumHp());
    auto history = array("advancementHistory");
    history.append(
        QJsonObject{{"class", cls},
                    {"classLevel", n},
                    {"level", number("level")},
                    {"hpDie", hitDieResult},
                    {"hpBaseGain", qMax(1, hitDieResult + modifier(number("Constitution")))},
                    {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    character["advancementHistory"] = history;
    dirty = true;
    if (!save())
    {
        character = before;
        loadWidgets();
        dirty = true;
        error = "Could not save the advancement.";
        return false;
    }
    loadWidgets();
    return true;
}
QString featureReference(const QString &cls, int n = 0) const
{
    QStringList lines;
    if (!subclassFor(cls).isEmpty() && classLevel(cls) >= 3)
        lines << "Subclass: " + subclassFor(cls) + "\n" +
                     subclassDescriptions().value(subclassFor(cls));
    for (auto v : advancementCatalog[cls].toObject()["features"].toArray())
    {
        auto f = v.toObject();
        if (n == 0 || f["level"].toInt() == n)
            lines << "Level " + QString::number(f["level"].toInt()) + ": " + f["name"].toString() +
                         "\n" + f["description"].toString();
    }
    return sourcesLast(lines.join("\n\n"));
}
bool chooseAdvanceItems(QString title, QStringList options, int count, QStringList &chosen,
                        const QMap<QString, QString> &descriptions = {}, int maximumCount = -1,
                        const QStringList &initial = {})
{
    if (options.size() < count)
    {
        displayError("There are not enough eligible choices. Check existing proficiencies or "
                     "previous selections.");
        return false;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(820, 650);
    auto *layout = new QVBoxLayout(&dialog);
    auto *label =
        new QLabel(QString("Choose exactly %1. Existing choices are not changed until you confirm.")
                       .arg(count));
    layout->addWidget(label);
    auto *searchBox = new QLineEdit;
    searchBox->setPlaceholderText("Search choices…");
    layout->addWidget(searchBox);
    auto *list = new QListWidget;
    list->setObjectName("levelUpChoices");
    list->setProperty("requiredChoiceCount", count);
    layout->addWidget(list, 1);
    for (auto option : options)
    {
        auto *item = new QListWidgetItem(option, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(initial.contains(option) ? Qt::Checked : Qt::Unchecked);
    }
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    layout->addWidget(details, 1);
    connect(list, &QListWidget::currentTextChanged, &dialog,
            [=](QString name)
            {
                QString text = descriptions.value(name);
                auto explanation = basicChoiceDescription(name);
                if (!explanation.isEmpty())
                    text = explanation + (text.isEmpty() ? QString() : "\n\n" + text);
                details->setPlainText(sourcesLast(text));
            });
    connect(searchBox, &QLineEdit::textChanged, &dialog,
            [=](QString term)
            {
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setHidden(
                        !list->item(i)->text().contains(term, Qt::CaseInsensitive));
            });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto updateCount = [=]
    {
        int checked = 0;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                ++checked;
        int maximum = maximumCount < 0 ? count : maximumCount;
        label->setText(
            QString("%1 / %2 selected. %3")
                .arg(checked)
                .arg(maximum)
                .arg(maximumCount < 0
                         ? QString("Choose exactly %1.").arg(count)
                         : QString("Choose between %1 and %2.").arg(count).arg(maximum)));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(checked >= count && checked <= maximum);
    };
    connect(list, &QListWidget::itemChanged, &dialog, updateCount);
    updateCount();
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [&]
            {
                QStringList selected;
                for (int i = 0; i < list->count(); ++i)
                    if (list->item(i)->checkState() == Qt::Checked)
                        selected << list->item(i)->text();
                if (selected.size() < count ||
                    selected.size() > (maximumCount < 0 ? count : maximumCount))
                {
                    QMessageBox::information(&dialog, "Choices remaining",
                                             QString("Choose exactly %1 entries.").arg(count));
                    return;
                }
                chosen = selected;
                dialog.accept();
            });
    list->setCurrentRow(0);
    return dialog.exec() == QDialog::Accepted;
}
bool chooseNewAdvanceItems(QString title, QStringList options, int total, QStringList &chosen,
                           const QMap<QString, QString> &descriptions, const QStringList &previous)
{
    chosen.clear();
    for (auto name : previous)
        if (options.contains(name) && !chosen.contains(name))
            chosen << name;
    if (chosen.size() >= total)
        return true;
    for (auto name : chosen)
        options.removeAll(name);
    QStringList additions;
    if (!chooseAdvanceItems(title + " — choose only new options", options, total - chosen.size(),
                            additions, descriptions))
        return false;
    chosen += additions;
    return true;
}
void showRuleDescription(const QString &title, const QString &text)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(650, 420);
    QVBoxLayout layout(&dialog);
    QPlainTextEdit details;
    details.setReadOnly(true);
    details.setPlainText(sourcesLast(text));
    layout.addWidget(&details);
    QDialogButtonBox buttons(QDialogButtonBox::Close);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout.addWidget(&buttons);
    dialog.exec();
}
bool advancementFeatEligible(const QJsonObject &f, bool style = false) const
{
    if (f["supplemental"].toBool() && !supplementalFeatEligible(f))
        return false;
    QString category = f["category"].toString(), name = f["name"].toString();
    if (style && category != "Fighting Style")
        return false;
    if (!style && category == "Fighting Style")
    {
        if (classLevel("Fighter") < 1 && classLevel("Paladin") < 2 && classLevel("Ranger") < 2)
            return false;
    }
    if (category == "General" && number("level") < 4)
        return false;
    if (category == "Epic Boon" && number("level") < 19)
        return false;
    if (!canSelectFeat(name))
        return false;
    QString desc = f["description"].toString();
    auto prereq = QRegularExpression("Prerequisites?: ([^\\n)]*)").match(desc).captured(1);
    QString abilityPart = prereq.section(',', 1).trimmed();
    if (abilityPart.contains("13+"))
    {
        bool matches = false, met = false;
        for (int i = 0; i < abilities.size(); ++i)
            if (abilityPart.contains(abilities[i]))
            {
                matches = true;
                met |= effectiveScore(i) >= 13;
            }
        if (matches && !met)
            return false;
    }
    if (prereq.contains("Spellcasting") || prereq.contains("Pact Magic"))
    {
        bool caster = false;
        for (auto v : classRoster())
        {
            auto c = v.toObject();
            caster |= classSpellLimit(c["name"].toString(), c["level"].toInt()) > 0 ||
                      c["subclass"] == "Eldritch Knight" || c["subclass"] == "Arcane Trickster";
        }
        if (!caster)
            return false;
    }
    return true;
}
bool resolveAdvanceTask(int index)
{
    auto tasks = array("advancementTasks");
    if (index < 0 || index >= tasks.size())
        return false;
    auto task = tasks[index].toObject();
    if (task["done"].toBool())
        return true;
    QString type = task["type"].toString(), cls = task["class"].toString(),
            title = task["title"].toString();
    int n = task["level"].toInt(), count = task["count"].toInt(1);
    QStringList chosen, options;
    QMap<QString, QString> descriptions;
    auto before = character;
    int previousMaximum = maximumHp(), previousCurrent = number("hpCurrent");
    if (type == "feat" || type == "style" || type == "origin")
    {
        for (auto v : featCatalog)
        {
            auto f = v.toObject();
            if (advancementFeatEligible(f, type == "style") &&
                (type != "origin" || f["category"] == "Origin"))
            {
                options << f["name"].toString();
                descriptions[f["name"].toString()] =
                    f["description"].toString() + "\n\n" + f["automation"].toString();
            }
        }
        if (type == "style" && (cls == "Paladin" || cls == "Ranger"))
        {
            QString name = cls == "Paladin" ? "Blessed Warrior" : "Druidic Warrior";
            options << name;
            descriptions[name] = "Learn two cantrips from the " +
                                 QString(cls == "Paladin" ? "Cleric" : "Druid") +
                                 " spell list instead of taking a Fighting Style feat.";
        }
        if (!chooseAdvanceItems(title, options, 1, chosen, descriptions))
            return false;
        if (chosen[0] == "Blessed Warrior" || chosen[0] == "Druidic Warrior")
            addAdvanceTask(tasks, cls, n, "cantrip", "Choose two " + chosen[0] + " cantrips", 2,
                           QJsonObject{{"spellClass", cls == "Paladin" ? "Cleric" : "Druid"}});
        else
        {
            QJsonObject feat;
            for (auto v : featCatalog)
                if (v.toObject()["name"].toString() == chosen[0])
                    feat = v.toObject();
            if (feat["description"].toString().contains("training", Qt::CaseInsensitive) &&
                QMessageBox::question(this, "Feat prerequisites",
                                      "Confirm that the character meets the armor or shield "
                                      "training prerequisite shown in the feat description.") !=
                    QMessageBox::Yes)
                return false;
            if (!configureFeat(feat))
                return false;
            feat["earnedAt"] = task["id"];
            auto feats = array("feats");
            feats.append(feat);
            character["feats"] = feats;
        }
    }
    else if (type == "skill" || type == "expertise")
    {
        QStringList allowed =
            type == "skill" && !task["anySkill"].toBool() ? classSkills(cls) : skillNames;
        if (type == "expertise" && cls == "Wizard")
            allowed =
                QStringList{"Arcana", "History", "Investigation", "Medicine", "Nature", "Religion"};
        for (auto skill : allowed)
        {
            int i = skillNames.indexOf(skill);
            int trained =
                effectTraining({"skill:*", "skill:" + skill}, array("skillProfs").at(i).toInt());
            if (type == "skill" ? trained == 0 : trained == 1)
                options << skill;
        }
        if (!chooseAdvanceItems(title, options, count, chosen))
            return false;
        auto profs = array("skillProfs");
        while (profs.size() < 18)
            profs.append(0);
        for (auto skill : chosen)
            profs[skillNames.indexOf(skill)] = type == "skill" ? 1 : 2;
        character["skillProfs"] = profs;
    }
    else if (type == "subclass")
    {
        options = subclasses.value(cls);
        for (auto name : options)
            descriptions[name] =
                name + " — " + cls + "\n\n" + subclassDescriptions().value(name) +
                "\n\nSubclass features begin at class level 3. This is a play-style summary; use "
                "the class rules for detailed actions and later features.";
        if (!chooseAdvanceItems(title, options, 1, chosen, descriptions))
            return false;
        if (cls == text("class"))
            character["subclass"] = chosen[0];
        else
        {
            auto roster = array("multiclasses");
            for (int i = 0; i < roster.size(); ++i)
            {
                auto c = roster[i].toObject();
                if (c["name"].toString() == cls)
                {
                    c["subclass"] = chosen[0];
                    roster[i] = c;
                }
            }
            character["multiclasses"] = roster;
        }
        if (cls == "Fighter" && chosen[0] == "Champion" && n >= 7)
            addAdvanceTask(tasks, cls, n, "style", "Champion: additional Fighting Style");
        if (cls == "Bard" && chosen[0] == "College of Lore")
            addAdvanceTask(tasks, cls, n, "skill", "Bonus Proficiencies: choose three skills", 3,
                           QJsonObject{{"anySkill", true}});
        queueSubclassReview(tasks, cls, n);
        if (cls == "Monk" && chosen[0] == "Warrior of the Mystic Arts")
            addAdvanceTask(tasks, cls, n, "spell", "Choose three Mystic Arts spells", 3,
                           QJsonObject{{"spellClass", "Sorcerer"}, {"maxSpellLevel", 1}});
        if ((cls == "Fighter" && chosen[0] == "Eldritch Knight") ||
            (cls == "Rogue" && chosen[0] == "Arcane Trickster"))
        {
            addAdvanceTask(tasks, cls, n, "cantrip", "Choose subclass Wizard cantrips", 2,
                           QJsonObject{{"spellClass", "Wizard"}});
            if (cls == "Rogue")
                addClassSpell("Mage Hand", cls);
            addAdvanceTask(tasks, cls, n, "spell", "Choose three subclass Wizard spells", 3,
                           QJsonObject{{"spellClass", "Wizard"}, {"maxSpellLevel", 1}});
        }
    }
    else if (type == "subclassFeatures")
    {
        bool oldDirty = dirty, oldLoading = loading;
        autosave.stop();
        loading = true;
        bool accepted = configureSubclassFeatures(cls, n, chosen, true);
        loading = oldLoading;
        dirty = oldDirty;
        if (!accepted)
        {
            character = before;
            loadWidgets();
            dirty = oldDirty;
            if (dirty)
                autosave.start(600);
            return false;
        }
    }
    else if (type == "language")
    {
        options = {"Common Sign Language",
                   "Draconic",
                   "Dwarvish",
                   "Elvish",
                   "Giant",
                   "Gnomish",
                   "Goblin",
                   "Halfling",
                   "Orc",
                   "Abyssal",
                   "Celestial",
                   "Deep Speech",
                   "Infernal",
                   "Primordial",
                   "Sylvan",
                   "Undercommon"};
        auto existing = text("languages").split(", ", Qt::SkipEmptyParts);
        for (auto language : existing)
            options.removeAll(language);
        if (!chooseAdvanceItems(title, options, count, chosen))
            return false;
        existing.append(chosen);
        character["languages"] = existing.join(", ");
    }
    else if (type == "order")
    {
        options = cls == "Cleric" ? QStringList{"Protector", "Thaumaturge"}
                                  : QStringList{"Warden", "Magician"};
        for (auto name : options)
            descriptions[name] = featureReference(cls, 1);
        if (!chooseAdvanceItems(title, options, 1, chosen, descriptions))
            return false;
        auto choices = character["classChoices"].toObject();
        choices[cls + ":order"] = chosen[0];
        character["classChoices"] = choices;
        if (chosen[0] == "Magician" || chosen[0] == "Thaumaturge")
            addAdvanceTask(tasks, cls, n, "cantrip", "Choose extra Order cantrip");
    }
    else if (type == "spell" || type == "cantrip" || type == "arcanum")
    {
        QString spellClass = task["spellClass"].toString(cls);
        int maximum = task["maxSpellLevel"].toInt(classSpellLimit(cls, n));
        if (maximum == 0 && (cls == "Fighter" || cls == "Rogue"))
            maximum = qMin(4, (n + 2) / 3);
        for (auto v : catalog["spells"].toArray())
        {
            auto s = v.toObject();
            if (s["legacyPdf"].toBool() && !character["allowLegacyPdfRules"].toBool())
                continue;
            bool classMatch = spellBelongsToClass(s, spellClass);
            if (task["anyClass"].toBool())
                classMatch = true;
            if (cls == "Bard" && n >= 10)
                for (auto other : QStringList{"Cleric", "Druid", "Wizard"})
                    classMatch |= s["meta"].toString().contains(other);
            int level = s["level"].toInt();
            bool eligible = type == "cantrip"   ? level == 0
                            : type == "arcanum" ? level == task["spellLevel"].toInt()
                                                : level > 0 && level <= maximum;
            if (task["ritualOnly"].toBool() && !s["description"].toString().contains("Ritual"))
                eligible = false;
            if (!task["school"].toString().isEmpty() &&
                !s["meta"].toString().contains(task["school"].toString()))
                eligible = false;
            if (classMatch && eligible && !alreadyKnowsSpell(s["name"].toString()) &&
                !alwaysPrepared(s["name"].toString(), cls))
            {
                options << s["name"].toString();
                descriptions[s["name"].toString()] = s["description"].toString();
            }
        }
        if (!chooseAdvanceItems(title, options, count, chosen, descriptions))
            return false;
        for (auto name : chosen)
            addClassSpell(name, cls, cls != "Wizard" || type == "cantrip");
        if (type == "arcanum")
        {
            auto arcanum = character["mysticArcanum"].toObject();
            arcanum[QString::number(task["spellLevel"].toInt())] = chosen[0];
            character["mysticArcanum"] = arcanum;
        }
    }
    else if (type == "options")
    {
        auto owned = character["classOptions"].toObject()[cls].toArray();
        for (auto v : advancementCatalog[cls].toObject()["options"].toArray())
        {
            auto o = v.toObject();
            QString name = o["name"].toString();
            if (o["minLevel"].toInt() > n || (owned.contains(name) && !o["repeatable"].toBool()))
                continue;
            bool met = true;
            for (auto need : o["requires"].toArray())
                if (!owned.contains(need))
                    met = false;
            if (!met)
                continue;
            options << name;
            descriptions[name] = o["description"].toString();
        }
        if (!chooseAdvanceItems(title, options, count, chosen, descriptions))
            return false;
        for (auto name : chosen)
        {
            owned.append(name);
            if (name == "Lessons of the First Ones")
                addAdvanceTask(tasks, cls, n, "origin",
                               "Choose Origin feat from Lessons of the First Ones");
            if (name == "Pact of the Chain")
                addClassSpell("Find Familiar", cls);
            if (name == "Pact of the Tome")
            {
                addAdvanceTask(tasks, cls, n, "cantrip", "Book of Shadows: choose three cantrips",
                               3, QJsonObject{{"anyClass", true}});
                addAdvanceTask(
                    tasks, cls, n, "spell", "Book of Shadows: choose two level 1 Ritual spells", 2,
                    QJsonObject{{"anyClass", true}, {"ritualOnly", true}, {"maxSpellLevel", 1}});
            }
        }
        if (cls == "Warlock")
            addAdvanceTask(tasks, cls, n, "review",
                           "Review invocation targets, prerequisites and conditional effects");
        auto all = character["classOptions"].toObject();
        all[cls] = owned;
        character["classOptions"] = all;
    }
    else
    {
        bool wasDirty = dirty, wasLoading = loading;
        autosave.stop();
        loading = true;
        dirty = false;
        bool accepted = title.contains("prepared spells", Qt::CaseInsensitive)
                            ? prepareLevelSpells(cls, n, chosen)
                            : reviewLevelFeatures(cls, n, title, chosen);
        loading = wasLoading;
        dirty = wasDirty;
        if (!accepted)
        {
            character = before;
            loadWidgets();
            dirty = wasDirty;
            if (dirty)
                autosave.start(600);
            return false;
        }
    }
    task["done"] = true;
    task["result"] = chosen.join(", ");
    tasks[index] = task;
    character["advancementTasks"] = tasks;
    auto oldSpent = array("slotsUsed");
    int oldPact = number("pactUsed");
    loadWidgets();
    configureSlots(false);
    character["slotsUsed"] = oldSpent;
    character["pactUsed"] = oldPact;
    if (previousCurrent > 0)
        character["hpCurrent"] =
            qBound(0, previousCurrent + maximumHp() - previousMaximum, maximumHp());
    dirty = true;
    if (!save())
    {
        character = before;
        loadWidgets();
        dirty = true;
        displayError("Could not save this choice.");
        return false;
    }
    loadWidgets();
    return true;
}
QJsonArray classResources() const
{
    QJsonArray rows;
    if (!character["manageClassProgression"].toBool())
        return rows;
    auto add = [&](QString cls, QString name, int maximum, QString recovery = "Long rest")
    {
        if (maximum > 0)
            rows.append(QJsonObject{{"key", cls + ":" + name},
                                    {"class", cls},
                                    {"name", name},
                                    {"max", maximum},
                                    {"recovery", recovery}});
    };
    int n = classLevel("Barbarian");
    if (n)
        add("Barbarian", "Rages", classStat("Barbarian", "Rages"),
            "One on short rest; all on long rest");
    n = classLevel("Bard");
    if (n)
        add("Bard", "Bardic Inspiration", qMax(1, ability(5)),
            n >= 5 ? "Short or long rest" : "Long rest");
    n = classLevel("Cleric");
    if (n >= 2)
        add("Cleric", "Channel Divinity", classStat("Cleric", "Channel Divinity"),
            "One on short rest; all on long rest");
    n = classLevel("Druid");
    if (n >= 2)
        add("Druid", "Wild Shape", classStat("Druid", "Wild Shape"),
            "One on short rest; all on long rest");
    n = classLevel("Fighter");
    if (n)
    {
        add("Fighter", "Second Wind", classStat("Fighter", "Second Wind"),
            "One on short rest; all on long rest");
        if (n >= 2)
            add("Fighter", "Action Surge", n >= 17 ? 2 : 1, "Short or long rest");
        if (n >= 9)
            add("Fighter", "Indomitable", n >= 17 ? 3 : n >= 13 ? 2 : 1);
    }
    n = classLevel("Monk");
    if (n >= 2)
    {
        add("Monk", "Focus Points", n, "Short or long rest");
        add("Monk", "Uncanny Metabolism", 1);
    }
    n = classLevel("Paladin");
    if (n)
    {
        add("Paladin", "Lay On Hands HP", 5 * n);
        if (n >= 3)
            add("Paladin", "Channel Divinity", n >= 11 ? 3 : 2,
                "One on short rest; all on long rest");
    }
    n = classLevel("Ranger");
    if (n)
    {
        add("Ranger", "Favored Enemy free casts", classStat("Ranger", "Favored Enemy"));
        if (n >= 10)
            add("Ranger", "Tireless", qMax(1, ability(4)));
        if (n >= 14)
            add("Ranger", "Nature's Veil", qMax(1, ability(4)));
    }
    n = classLevel("Rogue");
    if (n >= 20)
        add("Rogue", "Stroke of Luck", 1, "Short or long rest");
    n = classLevel("Sorcerer");
    if (n)
    {
        add("Sorcerer", "Innate Sorcery", 2);
        if (n >= 2)
            add("Sorcerer", "Sorcery Points", n);
    }
    n = classLevel("Warlock");
    if (n >= 2)
        add("Warlock", "Magical Cunning", 1);
    if (n >= 20)
        add("Warlock", "Eldritch Master", 1);
    auto arcanum = character["mysticArcanum"].toObject();
    for (auto key : arcanum.keys())
        add("Warlock", "Arcanum " + key + ": " + arcanum[key].toString(), 1);
    if (classLevel("Wizard"))
        add("Wizard", "Arcane Recovery", 1);
    n = classLevel("Artificer");
    if (n > 0)
        add("Artificer", "Tinker's Magic", qMax(1, ability(3)));
    if (n >= 7)
        add("Artificer", "Flash of Genius", qMax(1, ability(3)),
            n >= 20   ? "Short or long rest"
            : n >= 14 ? "One on short rest; all on long rest"
                      : "Long rest");
    for (auto value : subclassResources())
        rows.append(value);
    for (auto value : featureResources())
        rows.append(value);
    for (auto name :
         jsonStrings(character["classChoices"].toObject()["Wizard:Signature Spells"].toArray()))
        add("Wizard", "Signature spell: " + name, 1, "Short or long rest");
    return rows;
}
void restoreClassResources(bool longRest)
{
    auto spent = character["classResourcesSpent"].toObject();
    for (auto v : classResources())
    {
        auto resource = v.toObject();
        QString key = resource["key"].toString(), recovery = resource["recovery"].toString();
        if (resource["countBased"].toBool())
            continue;
        if (longRest || recovery == "Short or long rest")
            spent[key] = 0;
        else if (recovery.startsWith("One on short rest"))
            spent[key] = qMax(0, spent[key].toInt() - 1);
    }
    auto choices = character["classChoices"].toObject();
    for (auto key : choices.keys())
        if (key.endsWith(":Bladesinger:Bladesong state"))
            choices[key] = QJsonArray{"Inactive"};
    character["classChoices"] = choices;
    restSubclassFeatures(longRest);
    put("classResourcesSpent", spent);
    if (!longRest && classLevel("Warlock") > 0)
    {
        setNumber("pactUsed", 0);
        populateSlots();
    }
    refreshAdvancement();
}
void refreshAdvancement()
{
    if (!advancementPage)
        return;
    QSignalBlocker a(advancementTasks), r(classResourceTable);
    advancementTasks->clear();
    int feats = 0, skills = 0;
    auto tasks = array("advancementTasks");
    for (int i = 0; i < tasks.size(); ++i)
    {
        auto t = tasks[i].toObject();
        auto history = array("advancementHistory");
        QString currentClass =
            history.isEmpty() ? text("class") : history.last().toObject()["class"].toString();
        int currentClassLevel = history.isEmpty() ? classLevel(currentClass)
                                                  : history.last().toObject()["classLevel"].toInt();
        if (t["class"].toString() != currentClass || t["level"].toInt() != currentClassLevel)
            continue;
        bool done = t["done"].toBool();
        QString type = t["type"].toString();
        auto *item = new QListWidgetItem((done ? "✓ " : "○ ") + t["class"].toString() + " " +
                                             QString::number(t["level"].toInt()) + ": " +
                                             t["title"].toString(),
                                         advancementTasks);
        item->setData(Qt::UserRole, i);
        item->setToolTip(t["result"].toString());
        if (!done)
        {
            if (type == "feat" || type == "style" || type == "origin")
                ++feats;
            if (type == "skill" || type == "expertise")
                skills += t["count"].toInt();
        }
    }
    QStringList dice, limits;
    for (auto v : classRoster())
    {
        auto c = v.toObject();
        QString cls = c["name"].toString();
        int n = c["level"].toInt();
        dice << QString("%1d%2 %3").arg(n).arg(hitDie(cls)).arg(cls);
        auto stats = classStep(cls, n)["stats"].toObject();
        QStringList values;
        for (auto key : stats.keys())
            if (!key.startsWith("Slot "))
                values << key + ": " + stats[key].toString();
        limits << cls + " " + QString::number(n) + " — " +
                      classStep(cls, n)["features"].toString() + "\n" + values.join(" · ");
    }
    advancementStatus->setText(
        QString("Level %1 · Proficiency %2 · %3 pending feat/style choice(s) · %4 pending "
                "skill/expertise choice(s)\nHit Dice: %5 · %6 spent\n%7 pending tasks. Complete "
                "them, then Finish level up to lock abilities.")
            .arg(number("level", 1))
            .arg(sign(pb()))
            .arg(feats)
            .arg(skills)
            .arg(dice.join(" + "))
            .arg(number("hitDiceSpent"))
            .arg(pendingAdvanceNames().size()));
    QStringList choices;
    auto chosen = character["classChoices"].toObject();
    for (auto key : chosen.keys())
        choices << QString(key).replace(':', " — ") + ": " +
                       (chosen[key].isArray() ? jsonStrings(chosen[key].toArray()).join(", ")
                                              : chosen[key].toString());
    auto options = character["classOptions"].toObject();
    auto masteries = character["classMasteries"].toObject();
    for (auto cls : masteries.keys())
        choices << cls + " weapon masteries: " + jsonStrings(masteries[cls].toArray()).join(", ");
    for (auto cls : options.keys())
    {
        QStringList names;
        for (auto v : options[cls].toArray())
            names << v.toString();
        choices << cls + ": " + names.join(", ");
    }
    advancementDetails->setPlainText(
        limits.join("\n\n") + "\n\nChoices:\n" + choices.join("\n") +
        "\n\nProficiency is based on total character level. Class features and feat grants use "
        "class levels. No general skill-point or feat-point pool is awarded. Conditional actions "
        "and non-SRD subclass mechanics require review in the checklist.");
    auto resources = classResources();
    classResourceTable->setRowCount(resources.size());
    auto spent = character["classResourcesSpent"].toObject();
    for (int row = 0; row < resources.size(); ++row)
    {
        auto o = resources[row].toObject();
        int used = trackedResourceSpent(o);
        QStringList cells{o["class"].toString() + ": " + o["name"].toString(),
                          QString::number(o["max"].toInt()), QString::number(used),
                          QString::number(qMax(0, o["max"].toInt() - used)),
                          o["recovery"].toString()};
        for (int col = 0; col < cells.size(); ++col)
        {
            auto *item = new QTableWidgetItem(cells[col]);
            if (col != 2)
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            classResourceTable->setItem(row, col, item);
        }
    }
}
void showAdvancement()
{
    refreshAdvancement();
    tabs->setCurrentWidget(advancementPage);
}
void buildAdvancementUi()
{
    QVBoxLayout *body;
    advancementPage = page(body);
    advancementStatus = new QLabel;
    advancementStatus->setWordWrap(true);
    body->addWidget(advancementStatus);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(button("Resolve selected choice",
                              [this]
                              {
                                  auto *item = advancementTasks->currentItem();
                                  if (item)
                                  {
                                      int row = item->data(Qt::UserRole).toInt();
                                      resolveAdvanceTask(row);
                                      refreshAdvancement();
                                  }
                              }));
    buttons->addWidget(button("Finish level up & lock",
                              [this]
                              {
                                  if (!pendingAdvanceNames().isEmpty())
                                  {
                                      displayError("Complete the pending advancement tasks first.");
                                      return;
                                  }
                                  if (!lockAbilities())
                                      displayError("Could not save the character.");
                                  refreshAdvancement();
                              }));
    buttons->addWidget(
        button("Class rules reference",
               [this]
               {
                   bool ok;
                   QString cls =
                       QInputDialog::getItem(this, "Class rules", "Class", classes,
                                             qMax(0, classes.indexOf(text("class"))), false, &ok);
                   if (!ok)
                       return;
                   QDialog d(this);
                   d.setWindowTitle(cls + " — SRD 5.2.1");
                   d.resize(850, 700);
                   auto *l = new QVBoxLayout(&d);
                   auto *t = new QPlainTextEdit;
                   t->setReadOnly(true);
                   t->setPlainText(advancementCatalog[cls].toObject()["reference"].toString());
                   l->addWidget(t);
                   auto *b = new QDialogButtonBox(QDialogButtonBox::Close);
                   connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
                   l->addWidget(b);
                   d.exec();
               }));
    body->addLayout(buttons);
    advancementTasks = new QListWidget;
    advancementTasks->setMinimumHeight(150);
    body->addWidget(advancementTasks, 1);
    advancementDetails = new QPlainTextEdit;
    advancementDetails->setReadOnly(true);
    advancementDetails->setMaximumHeight(160);
    body->addWidget(advancementDetails);
    body->addWidget(new QLabel("Class resources — edit Spent when you use a feature"));
    classResourceTable = table({"Resource", "Maximum", "Spent", "Remaining", "Recovery"}, true);
    body->addWidget(classResourceTable, 1);
    connect(classResourceTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *item)
            {
                if (loading || item->column() != 2)
                    return;
                bool ok;
                int value = item->text().toInt(&ok);
                auto resource = classResources()[item->row()].toObject();
                if (!ok || value < 0 || value > resource["max"].toInt())
                {
                    QTimer::singleShot(0, this, [this] { refreshAdvancement(); });
                    return;
                }
                setTrackedResourceSpent(resource, value);
                dirty = true;
                autosave.start(600);
                saveState->setText("Saving…");
                QTimer::singleShot(0, this, [this] { refreshDerived(); });
            });
    auto *rest = new QHBoxLayout;
    rest->addWidget(button("Short rest: recover class resources",
                           [this]
                           {
                               restoreClassResources(false);
                               log("Short rest",
                                   "Eligible tracked class resources recovered. Roll spent Hit "
                                   "Dice separately. Pact Magic slots also recovered.");
                           }));
    auto *active = new QCheckBox("Situational class traits active (not Incapacitated)");
    active->setChecked(true);
    connect(active, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                if (!loading)
                    put("classTraitsActive", checked);
            });
    classTraitsCheck = active;
    rest->addWidget(active);
    body->addLayout(rest);
    tabs->addTab(advancementPage, "Level up & class");
}
void guidedLevelUp()
{
    if (!creationProblem().isEmpty() && !configureStartingProficiencies())
        return;
    if (number("level", 1) >= 20)
    {
        displayError("Characters are limited to level 20.");
        return;
    }
    if (!pendingAdvanceNames().isEmpty())
    {
        showAdvancement();
        return;
    }
    if (!save())
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Level up — 2024 rules");
    dialog.resize(850, 680);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *cls = new QComboBox;
    cls->addItems(classes);
    cls->setCurrentText(text("class"));
    form->addRow("Gain one level in", cls);
    auto *hp = new QComboBox;
    hp->addItems({"Fixed average (recommended)", "Roll the class Hit Die"});
    form->addRow("Hit point increase", hp);
    layout->addLayout(form);
    auto *preview = new QPlainTextEdit;
    preview->setReadOnly(true);
    layout->addWidget(preview, 1);
    auto update = [=]
    {
        QString name = cls->currentText();
        int n = classLevel(name) + 1;
        QStringList stats;
        auto step = classStep(name, n);
        auto values = step["stats"].toObject();
        for (auto key : values.keys())
            stats << key + ": " + values[key].toString();
        QString problem = multiclassProblem(name);
        preview->setPlainText(
            QString("Character level %1 → %2\n%3 class level %4 → %5\nProficiency %6 → %7\nHP: d%8 "
                    "+ Constitution modifier (%9), minimum +1; fixed average die is %10.\nGain one "
                    "d%8 Hit Die. Current damage and spent slots/resources are "
                    "preserved.\n\n%11\n\n%12\n\n%13\n\n%14\n\nClass effects will be tracked "
                    "automatically. Remove any manually baked-in class speed, saving-throw or "
                    "unarmored-AC bonuses to avoid duplicates. Complete earned choices on Level up "
                    "& class, then Finish level up.")
                .arg(number("level", 1))
                .arg(number("level", 1) + 1)
                .arg(name)
                .arg(n - 1)
                .arg(n)
                .arg(sign(pb()))
                .arg(sign(proficiency(number("level", 1) + 1)))
                .arg(hitDie(name))
                .arg(sign(ability(2)))
                .arg(hitDie(name) / 2 + 1)
                .arg(problem.isEmpty() ? "Multiclass requirements met." : problem)
                .arg(step["features"].toString(), stats.join(" · "), featureReference(name, n)));
    };
    connect(cls, &QComboBox::currentTextChanged, &dialog, update);
    update();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply level & choose features");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [&]
            {
                QString problem = multiclassProblem(cls->currentText());
                if (!problem.isEmpty())
                {
                    QMessageBox::information(&dialog, "Multiclass requirements", problem);
                    return;
                }
                dialog.accept();
            });
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString error;
    int result = hp->currentIndex() == 0 ? hitDie(cls->currentText()) / 2 + 1
                                         : die(hitDie(cls->currentText()));
    if (!applyLevelGain(cls->currentText(), result, error))
    {
        displayError(error);
        return;
    }
    showAdvancement();
}
