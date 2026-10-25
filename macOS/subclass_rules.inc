bool isThirdCaster(const QString &cls) const
{
    QString name = subclassFor(cls);
    return (cls == "Fighter" && name == "Eldritch Knight") ||
           (cls == "Rogue" && name == "Arcane Trickster") ||
           (cls == "Monk" && name == "Warrior of the Mystic Arts");
}
int thirdCasterPrepared(int level) const
{
    static const int count[] = {3, 4, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11, 11, 11, 12, 13};
    return level < 3 ? 0 : count[qBound(3, level, 20) - 3];
}
QString thirdCasterList(const QString &cls) const
{
    return cls == "Monk" ? "Sorcerer" : "Wizard";
}
QString subclassFor(const QString &cls) const
{
    for (auto value : classRoster())
        if (value.toObject()["name"].toString() == cls)
            return value.toObject()["subclass"].toString();
    return {};
}
QJsonObject subclassDefinition(const QString &cls) const
{
    QString name = subclassFor(cls);
    QJsonObject result;
    for (auto value : supplementalSubclassCatalog())
        if (value.toObject()["className"] == cls && value.toObject()["name"] == name)
            result = value.toObject();
    auto expanded = featureSubclassDefinition(cls, name);
    if (!expanded.isEmpty())
    {
        if (result.isEmpty())
        {
            result["name"] = name;
            result["className"] = cls;
            result["source"] = expanded["source"];
            result["url"] = expanded["sourceUrl"];
        }
        result["milestones"] = expanded["milestones"];
        if (expanded.contains("savantSchool"))
            result["savantSchool"] = expanded["savantSchool"];
    }
    return result;
}
QString subclassChoiceKey(const QString &cls, const QJsonObject &definition,
                          const QString &key) const
{
    return "Subclass:" + cls + ":" + definition["name"].toString() + ":" + key;
}
QString normalizedSpell(QString name) const
{
    return name.replace(QChar(0x2019), '\'').remove('\'').trimmed().toCaseFolded();
}
QString canonicalSpell(const QString &name) const
{
    for (auto value : catalog["spells"].toArray())
        if (normalizedSpell(value.toObject()["name"].toString()) == normalizedSpell(name))
            return value.toObject()["name"].toString();
    return {};
}
bool spellBelongsToClass(const QJsonObject &spell, const QString &cls) const
{
    if (spell["legacyPdf"].toBool() && !character["allowLegacyPdfRules"].toBool())
        return false;
    if (cls == "Artificer")
    {
        QString rulesName = spell["rulesName"].toString(spell["name"].toString());
        for (auto name : artificerReference()["spells"].toArray())
            if (normalizedSpell(name.toString()) == normalizedSpell(rulesName))
                return true;
        return false;
    }
    if (isThirdCaster(cls) && spell["meta"].toString().contains(thirdCasterList(cls)))
        return true;
    if (spell["meta"].toString().contains(cls))
        return true;
    if (classLevel(cls) >= 3)
        for (auto value : subclassDefinition(cls)["spellLists"].toArray())
            if (spell["meta"].toString().contains(value.toString()))
                return true;
    return false;
}
QStringList subclassSpellNames(const QString &cls) const
{
    auto definition = subclassDefinition(cls);
    int level = classLevel(cls);
    QStringList names;
    if (level < 3)
        return names;
    for (auto value : definition["grants"].toArray())
    {
        auto grant = value.toObject();
        if (level >= grant["level"].toInt())
            names += jsonStrings(grant["names"].toArray());
    }
    auto saved = character["classChoices"].toObject();
    for (auto value : definition["choices"].toArray())
    {
        auto choice = value.toObject();
        if (level < choice["level"].toInt(3))
            continue;
        auto chosen = jsonStrings(
            saved[subclassChoiceKey(cls, definition, choice["key"].toString())].toArray());
        if (choice.contains("spellClass"))
            names += chosen;
        for (auto optionValue : choice["options"].toArray())
        {
            auto option = optionValue.toObject();
            if (chosen.contains(option["name"].toString()))
                names += jsonStrings(option["spells"].toArray());
        }
    }
    for (auto v : classFeatureEntries(cls, level))
    {
        auto f = v.toObject();
        for (auto g : f["grants"].toArray())
            if (level >= g.toObject()["level"].toInt())
                names += jsonStrings(g.toObject()["names"].toArray());
        names += jsonStrings(f["spells"].toArray());
    }
    names.removeDuplicates();
    return names;
}
void syncSubclassSpells()
{
    // Only entries created by this feature may be removed on a subclass change.
    QMap<QString, QPair<QString, QString>> expected;
    for (auto value : classRoster())
    {
        QString cls = value.toObject()["name"].toString();
        auto names = subclassSpellNames(cls);
        if (cls == "Artificer" && classLevel(cls) > 0)
            names << "Mending";
        for (auto name : names)
        {
            auto canonical = canonicalSpell(name);
            if (!canonical.isEmpty())
                expected[cls + ":" + canonical] = {cls, canonical};
        }
    }
    auto spells = array("spells");
    for (int i = spells.size() - 1; i >= 0; --i)
    {
        auto entry = spells[i].toObject();
        if (entry.contains("subclassGrantKey") &&
            !expected.contains(entry["subclassGrantKey"].toString()))
            spells.removeAt(i);
    }
    for (auto it = expected.begin(); it != expected.end(); ++it)
    {
        bool found = false;
        for (auto value : spells)
        {
            auto entry = value.toObject();
            if (entry["name"].toString() == it.value().second &&
                entry["learnedFrom"].toString() == it.value().first)
                found = true;
        }
        if (!found)
            spells.append(
                QJsonObject{{"name", it.value().second},
                            {"prepared", true},
                            {"learnedFrom", it.value().first},
                            {"castingAbility", it.value().first == "Rogue" &&
                                                       subclassFor("Rogue") == "Phantom" &&
                                                       it.value().second == "Speak with Dead"
                                                   ? "Dexterity"
                                                   : castingFor(it.value().first)},
                            {"subclassGrantKey", it.key()}});
    }
    character["spells"] = spells;
}
QJsonArray subclassEffects() const
{
    QJsonArray result;
    if (!character["manageClassProgression"].toBool())
        return result;
    for (auto value : classRoster())
    {
        auto roster = value.toObject();
        QString cls = roster["name"].toString();
        int level = roster["level"].toInt();
        if (level < 3)
            continue;
        auto definition = subclassDefinition(cls);
        auto add = [&](QJsonArray effects)
        {
            for (auto effectValue : effects)
            {
                auto effect = effectValue.toObject();
                if (level < effect["level"].toInt(3))
                    continue;
                if (effect["condition"] == "heavyArmor" &&
                    wornCatalogArmor()["armorCategory"] != "Heavy Armor")
                    continue;
                if (effect["condition"] == "wearingArmor" && wornCatalogArmor().isEmpty() &&
                    !character["wearingArmor"].toBool())
                    continue;
                if (definition["name"] == "Bladesinger")
                {
                    QString armorCategory = wornCatalogArmor()["armorCategory"].toString();
                    if (!wornCatalogArmor(true).isEmpty() || armorCategory == "Medium Armor" ||
                        armorCategory == "Heavy Armor" ||
                        (character["wearingArmor"].toBool() && text("armorDexMode") != "Full"))
                        continue;
                }
                effect["source"] = cls + ": " + definition["name"].toString();
                result.append(effect);
            }
        };
        add(definition["effects"].toArray());
        auto saved = character["classChoices"].toObject();
        for (auto choiceValue : definition["choices"].toArray())
        {
            auto choice = choiceValue.toObject();
            if (level < choice["level"].toInt(3))
                continue;
            auto selected =
                saved[subclassChoiceKey(cls, definition, choice["key"].toString())].toArray();
            for (auto optionValue : choice["options"].toArray())
                if (selected.contains(optionValue.toObject()["name"]))
                    add(optionValue.toObject()["effects"].toArray());
        }
    }
    return result;
}
QJsonArray subclassResources() const
{
    QJsonArray result;
    for (auto value : classRoster())
    {
        QString cls = value.toObject()["name"].toString();
        int level = classLevel(cls);
        auto definition = subclassDefinition(cls);
        for (auto resourceValue : definition["resources"].toArray())
        {
            auto resource = resourceValue.toObject();
            if (level < resource["level"].toInt(3))
                continue;
            QString scale = resource["scale"].toString();
            int maximum = scale == "classLevel"    ? level
                          : scale == "proficiency" ? pb()
                          : abilities.contains(scale)
                              ? qMax(resource["minimum"].toInt(1), modifier(number(scale)))
                              : resource["value"].toInt();
            resource["key"] = "Subclass:" + cls + ":" + definition["name"].toString() + ":" +
                              resource["name"].toString();
            resource["class"] = cls;
            resource["max"] = maximum;
            result.append(resource);
        }
    }
    return result;
}
void queueSubclassReview(QJsonArray &tasks, const QString &cls, int level)
{
    auto definition = subclassDefinition(cls);
    if (definition.isEmpty() || level < 3)
        return;
    bool due = definition["milestones"].toArray().contains(level);
    for (auto value : definition["choices"].toArray())
        due |= value.toObject()["level"].toInt(3) == level;
    QString school = definition["savantSchool"].toString();
    if (!school.isEmpty() && (level == 3 || QList<int>{5, 7, 9, 11, 13, 15, 17}.contains(level)))
    {
        bool exists = false;
        for (auto taskValue : tasks)
            if (taskValue.toObject()["savantSchool"] == school &&
                taskValue.toObject()["class"] == cls && taskValue.toObject()["level"] == level)
                exists = true;
        if (!exists)
            addAdvanceTask(
                tasks, cls, level, "spell", school + " Savant: choose bonus spellbook spells",
                level == 3 ? 2 : 1,
                QJsonObject{{"spellClass", "Wizard"},
                            {"maxSpellLevel", level == 3 ? 2 : classSpellLimit(cls, level)},
                            {"school", school},
                            {"savantSchool", school}});
    }
    if (!due)
        return;
    for (auto value : tasks)
        if (value.toObject()["type"] == "subclassFeatures" && value.toObject()["class"] == cls &&
            value.toObject()["level"] == level)
            return;
    addAdvanceTask(tasks, cls, level, "subclassFeatures",
                   "Review " + definition["name"].toString() + " features & choices");
}
bool configureSubclassFeatures(const QString &cls, int level, QStringList &result,
                               bool advancementOnly = false)
{
    auto definition = subclassDefinition(cls);
    QString name = subclassFor(cls);
    if (name.isEmpty() || level < 3)
    {
        displayError("Choose a subclass at class level 3 before configuring its features.");
        return false;
    }
    auto before = character;
    auto saved = character["classChoices"].toObject();
    for (auto value : definition["choices"].toArray())
    {
        auto choice = value.toObject();
        if (level < choice["level"].toInt(3))
            continue;
        QString key = subclassChoiceKey(cls, definition, choice["key"].toString());
        QStringList options, picked;
        QMap<QString, QString> descriptions;
        if (choice.contains("spellClass"))
        {
            for (auto spellValue : catalog["spells"].toArray())
            {
                auto spell = spellValue.toObject();
                QString spellName = spell["name"].toString();
                if (spell["level"].toInt() != choice["spellLevel"].toInt() ||
                    !spellBelongsToClass(spell, choice["spellClass"].toString()))
                    continue;
                if (alreadyKnowsSpell(spellName) && !saved[key].toArray().contains(spellName))
                    continue;
                options << spellName;
                descriptions[spellName] = spell["description"].toString();
            }
        }
        else
            for (auto optionValue : choice["options"].toArray())
            {
                auto option = optionValue.toObject();
                if (level < option["level"].toInt(3))
                    continue;
                options << option["name"].toString();
                descriptions[option["name"].toString()] = option["description"].toString();
            }
        int required = choice["count"].toInt(1);
        auto counts = choice["countByLevel"].toObject();
        for (auto levelKey : counts.keys())
            if (level >= levelKey.toInt())
                required = qMax(required, counts[levelKey].toInt());
        auto prior = jsonStrings(saved[key].toArray());
        bool accepted = advancementOnly
                            ? chooseNewAdvanceItems(name + ": " + choice["key"].toString(), options,
                                                    required, picked, descriptions, prior)
                            : chooseAdvanceItems(name + ": " + choice["key"].toString(), options,
                                                 required, picked, descriptions, -1, prior);
        if (!accepted)
        {
            character = before;
            return false;
        }
        saved[key] = QJsonArray::fromStringList(picked);
        character["classChoices"] = saved;
        result << choice["key"].toString() + ": " + picked.join(", ");
    }
    for (auto v : classFeatureEntries(cls, level))
    {
        auto f = v.toObject();
        if (!featureChoicesComplete(f, level) && !configureIndividualFeature(f, advancementOnly))
        {
            character = before;
            return false;
        }
    }
    QStringList reviewed, entries;
    QMap<QString, QString> featureDetails;
    for (auto v : classFeatureEntries(cls, level))
    {
        auto f = v.toObject();
        if (advancementOnly && f["level"].toInt() != level)
            continue;
        QString title =
            "Level " + QString::number(f["level"].toInt()) + ": " + f["name"].toString();
        entries << title;
        featureDetails[title] = f["description"].toString() +
                                "\n\nUse Subclass abilities for choices, supported rolls, resource "
                                "counters, active effects and companion HP.\n" +
                                f["sourceUrl"].toString();
    }
    if (entries.isEmpty() && !advancementOnly)
    {
        entries << "Review " + name;
        featureDetails[entries[0]] = subclassDescriptions().value(name);
    }
    if (!entries.isEmpty() && !chooseAdvanceItems("Unlocked subclass abilities — " + name, entries,
                                                  entries.size(), reviewed, featureDetails))
    {
        character = before;
        return false;
    }
    syncSubclassSpells();
    result += reviewed;
    return true;
}
void editSubclassFeatures()
{
    QStringList options, selected;
    QMap<QString, QString> descriptions;
    for (auto value : classRoster())
    {
        auto entry = value.toObject();
        QString cls = entry["name"].toString();
        if (entry["level"].toInt() >= 3 && !entry["subclass"].toString().isEmpty())
        {
            options << cls;
            descriptions[cls] = subclassDescriptions().value(entry["subclass"].toString());
        }
    }
    if (options.isEmpty())
    {
        displayError("Select a subclass for a class at level 3 or higher first.");
        return;
    }
    if (options.size() == 1)
        selected = options;
    else if (!chooseAdvanceItems("Choose a class to configure", options, 1, selected, descriptions))
        return;
    auto before = character;
    bool oldDirty = dirty, oldLoading = loading;
    autosave.stop();
    loading = true;
    QStringList result;
    bool accepted = configureSubclassFeatures(selected[0], classLevel(selected[0]), result);
    loading = oldLoading;
    if (!accepted)
    {
        character = before;
        loadWidgets();
        dirty = oldDirty;
        if (dirty)
            autosave.start(600);
        return;
    }
    character["manageClassProgression"] = true;
    loadWidgets();
    dirty = true;
    if (!save())
    {
        character = before;
        loadWidgets();
        dirty = true;
        displayError("Could not save subclass choices. Previous choices were preserved.");
    }
}
bool selfTestExpandedSubclasses()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](QString reason)
    {
        QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (file.open(QIODevice::WriteOnly))
            file.write(("Expanded subclasses: " + reason).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    if (supplementalSubclassCatalog().size() != 82 || classes.size() != 13)
        return fail("catalog count");
    QSet<QString> seen;
    for (auto value : supplementalSubclassCatalog())
    {
        auto definition = value.toObject();
        QString name = definition["name"].toString(), cls = definition["className"].toString();
        if (!classes.contains(cls) || seen.contains(name) ||
            !subclasses.value(cls).contains(name) || definition["source"].toString().isEmpty() ||
            definition["url"].toString().isEmpty())
            return fail("catalog entry " + name);
        seen.insert(name);
    }
    auto fixture = [&](QString cls, QString subclass, int level)
    {
        character = blank();
        character["class"] = cls;
        character["subclass"] = subclass;
        character["primaryLevel"] = level;
        character["level"] = level;
        character["manageClassProgression"] = true;
        character["Charisma"] = 16;
        character["Intelligence"] = 16;
        loadWidgets();
    };
    fixture("Rogue", "Swashbuckler", 3);
    if (initiativeBonus() != 5)
        return fail("Swashbuckler initiative");
    character["multiclasses"] =
        QJsonArray{QJsonObject{{"name", "Wizard"}, {"level", 3}, {"subclass", "Chronurgy Magic"}}};
    character["level"] = 6;
    if (initiativeBonus() != 8)
        return fail("multiclass effects");
    fixture("Rogue", "Scout", 3);
    if (effectTraining({"skill:Nature"}, 0) != 2 || effectTraining({"skill:Survival"}, 0) != 2)
        return fail("Scout expertise");
    fixture("Rogue", "Scout", 9);
    if (effectiveSpeed() != 40)
        return fail("Scout mobility");
    character["subclass"] = "Thief";
    if (effectiveSpeed() != 30 || effectTraining({"skill:Nature"}, 0))
        return fail("stale subclass effects");
    fixture("Artificer", "", 1);
    configureSlots(false);
    if (classStat("Artificer", "Prepared spells") != 2 || classStat("Artificer", "Cantrips") != 2 ||
        array("slotsMax")[0].toInt() != 2 || !alreadyKnowsSpell("Mending") ||
        hitDie("Artificer") != 8)
        return fail("Artificer first level");
    if (!spellBelongsToClass(findSpell("Cure Wounds"), "Artificer") ||
        spellBelongsToClass(findSpell("Fireball"), "Artificer"))
        return fail("Artificer spell list");
    fixture("Artificer", "Alchemist", 13);
    configureSlots(false);
    if (array("slotsMax")[0].toInt() != 4 || array("slotsMax")[1].toInt() != 3 ||
        array("slotsMax")[2].toInt() != 3 || array("slotsMax")[3].toInt() != 1)
        return fail("Artificer spell slots");
    fixture("Artificer", "Alchemist", 3);
    character["multiclasses"] =
        QJsonArray{QJsonObject{{"name", "Wizard"}, {"level", 3}, {"subclass", "Evoker"}}};
    character["level"] = 6;
    configureSlots(false);
    if (array("slotsMax")[2].toInt() != 2)
        return fail("Artificer multiclass rounding");
    fixture("Monk", "Warrior of the Mystic Arts", 7);
    configureSlots(false);
    if (array("slotsMax")[1].toInt() != 2 || castingFor("Monk") != "Wisdom" ||
        classSpellLimit("Monk", 7) != 2 || !spellBelongsToClass(findSpell("Fire Bolt"), "Monk"))
        return fail("Mystic Arts spellcasting");
    fixture("Ranger", "Swarmkeeper", 3);
    if (!alreadyKnowsSpell("Mage Hand") || !alwaysPrepared("Faerie Fire", "Ranger"))
        return fail("subclass spell grants");
    addClassSpell("Magic Missile", "Ranger");
    character["subclass"] = "Hunter";
    loadWidgets();
    if (alreadyKnowsSpell("Mage Hand") || !alreadyKnowsSpell("Magic Missile"))
        return fail("grant cleanup changed learned spells");
    fixture("Wizard", "Conjurer", 3);
    QJsonArray tasks;
    queueSubclassReview(tasks, "Wizard", 3);
    queueSubclassReview(tasks, "Wizard", 3);
    int savant = 0, reviews = 0;
    for (auto task : tasks)
    {
        savant += task.toObject()["savantSchool"] == "Conjuration";
        reviews += task.toObject()["type"] == "subclassFeatures";
    }
    if (savant != 1 || reviews != 1)
        return fail("duplicate Savant or subclass tasks");
    fixture("Warlock", "Hexblade Patron", 3);
    QString resourceKey = "Subclass:Warlock:Hexblade Patron:Hexblades Curse";
    character["classResourcesSpent"] = QJsonObject{{resourceKey, 1}};
    restoreClassResources(false);
    if (character["classResourcesSpent"].toObject()[resourceKey].toInt() != 0)
        return fail("subclass resource recovery");
    fixture("Wizard", "Bladesinger", 3);
    bool activeChosen = false, reviewSeen = false;
    QTimer chooser;
    connect(&chooser, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                    return;
                for (int i = 0; i < list->count(); ++i)
                {
                    auto *item = list->item(i);
                    bool review = dialog->windowTitle().startsWith("Unlocked subclass abilities");
                    bool select =
                        item->text() == "Active" || review || item->text() == "Acrobatics";
                    item->setCheckState(select ? Qt::Checked : Qt::Unchecked);
                    if (item->text() == "Active")
                        activeChosen = true;
                    if (review)
                    {
                        reviewSeen = true;
                        list->setCurrentItem(item);
                    }
                }
                dialog->grab().save(QCoreApplication::applicationDirPath() +
                                    "/preview-subclass-features.png");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    chooser.start(30);
    QStringList result;
    bool accepted = configureSubclassFeatures("Wizard", 3, result);
    chooser.stop();
    if (!accepted || !activeChosen || !reviewSeen || effectBonus({"ac"}) != 3 ||
        effectiveSpeed() != 40)
        return fail("subclass choice modifiers");
    character = QJsonDocument::fromJson(QJsonDocument(character).toJson()).object();
    loadWidgets();
    if (effectBonus({"ac"}) != 3)
        return fail("choices lost on reload");
    auto beforeCancel = character;
    QTimer cancelChooser;
    connect(&cancelChooser, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                    return;
                if (dialog->windowTitle().startsWith("Unlocked subclass abilities"))
                {
                    dialog->reject();
                    return;
                }
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setCheckState(
                        list->item(i)->text() == "Inactive" ? Qt::Checked : Qt::Unchecked);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    cancelChooser.start(30);
    result.clear();
    accepted = configureSubclassFeatures("Wizard", 3, result);
    cancelChooser.stop();
    if (accepted || character != beforeCancel || effectBonus({"ac"}) != 3)
        return fail("cancel changed saved choices");
    restoreClassResources(true);
    if (effectBonus({"ac"}) != 0)
        return fail("Bladesong remained active after a rest");
    for (int runeLevel : {3, 7})
    {
        fixture("Fighter", "Rune Knight", runeLevel);
        bool correctOptions = false;
        QTimer runeChooser;
        connect(&runeChooser, &QTimer::timeout, this,
                [&]
                {
                    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    if (!dialog)
                        return;
                    auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                    if (!list)
                        return;
                    if (dialog->windowTitle().startsWith("Rune Knight:"))
                    {
                        correctOptions = list->count() == (runeLevel == 3 ? 4 : 6) &&
                                         list->property("requiredChoiceCount").toInt() ==
                                             (runeLevel == 3 ? 2 : 3);
                        for (int i = 0; i < list->count(); ++i)
                            list->item(i)->setCheckState(
                                i < (runeLevel == 3 ? 2 : 3) ? Qt::Checked : Qt::Unchecked);
                    }
                    else
                        for (int i = 0; i < list->count(); ++i)
                            list->item(i)->setCheckState(Qt::Checked);
                    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
                });
        runeChooser.start(30);
        result.clear();
        accepted = configureSubclassFeatures("Fighter", runeLevel, result);
        runeChooser.stop();
        if (!accepted || !correctOptions)
            return fail("Rune Knight level restrictions");
    }
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}
