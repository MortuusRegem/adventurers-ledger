bool selfTestLevelChoices()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](QString reason)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Checkbox choices: " + reason).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    character = blank();
    character["class"] = "Wizard";
    character["primaryLevel"] = 3;
    character["level"] = 3;
    character["manageClassProgression"] = true;
    addClassSpell("Shield", "Wizard");
    addClassSpell("Magic Missile", "Wizard");
    addClassSpell("Mage Armor", "Wizard");
    addClassSpell("Fireball", "Wizard");
    character["slotsUsed"] = QJsonArray{1, 1, 0, 0, 0, 0, 0, 0, 0};
    QJsonArray tasks;
    addAdvanceTask(tasks, "Wizard", 3, "review",
                   "Review prepared spells and optional spell replacements");
    character["advancementTasks"] = tasks;
    loadWidgets();
    bool checked = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!d)
                return;
            auto *list = d->findChild<QListWidget *>("levelUpChoices");
            if (!list)
            {
                d->reject();
                return;
            }
            bool illegal = false;
            for (int i = 0; i < list->count(); ++i)
            {
                auto *item = list->item(i);
                illegal |= item->text() == "Fireball";
                item->setCheckState(item->text() == "Shield" ? Qt::Checked : Qt::Unchecked);
            }
            for (auto *edit : d->findChildren<QPlainTextEdit *>())
                if (!edit->isReadOnly())
                    illegal = true;
            if (illegal)
            {
                d->reject();
                return;
            }
            checked = true;
            d->grab().save(QCoreApplication::applicationDirPath() +
                           "/preview-spell-checkboxes.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    if (!resolveAdvanceTask(0) || !checked)
        return fail("prepared spell picker");
    for (auto v : array("spells"))
    {
        auto s = v.toObject();
        if (s["name"] == "Shield" && !s["prepared"].toBool())
            return fail("selected spell not prepared");
        if (s["name"] == "Magic Missile" && s["prepared"].toBool())
            return fail("unchecked spell still prepared");
    }
    if (array("slotsUsed").at(0).toInt() != 1 || array("slotsUsed").at(1).toInt() != 1)
        return fail("spent slots changed");
    character = blank();
    character["class"] = "Cleric";
    character["primaryLevel"] = 7;
    character["level"] = 7;
    character["manageClassProgression"] = true;
    character["Wisdom"] = 16;
    character["spellAbility"] = "Wisdom";
    tasks = {};
    addAdvanceTask(tasks, "Cleric", 7, "review",
                   "Review new class and subclass features: Blessed Strikes");
    character["advancementTasks"] = tasks;
    loadWidgets();
    QTimer answer;
    int dialogs = 0;
    bool cancelSecond = false;
    connect(&answer, &QTimer::timeout, this,
            [&]
            {
                auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!d)
                    return;
                auto *list = d->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                {
                    d->reject();
                    return;
                }
                bool feature = d->windowTitle() == "Choose Blessed Strikes";
                if (cancelSecond && !feature)
                {
                    d->reject();
                    return;
                }
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setCheckState(
                        !feature || list->item(i)->text() ==
                                        (cancelSecond ? "Divine Strike" : "Potent Spellcasting")
                            ? Qt::Checked
                            : Qt::Unchecked);
                ++dialogs;
                d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    answer.start(10);
    if (!resolveAdvanceTask(0))
    {
        answer.stop();
        return fail("class feature checkboxes");
    }
    answer.stop();
    if (dialogs != 2 ||
        character["classChoices"].toObject()["Cleric:Blessed Strikes"] != "Potent Spellcasting" ||
        !spellFormula("Sacred Flame", 0, "Wisdom").contains("+3"))
        return fail("class feature not applied");
    auto task = tasks[0].toObject();
    task["done"] = false;
    tasks[0] = task;
    character["advancementTasks"] = tasks;
    auto before = character;
    cancelSecond = true;
    answer.start(10);
    bool accepted = resolveAdvanceTask(0);
    answer.stop();
    if (accepted || character != before)
        return fail("partial cancellation changed choices");
    character = blank();
    loadWidgets();
    QJsonObject skilled;
    for (auto v : featCatalog)
        if (v.toObject()["name"] == "Skilled")
            skilled = v.toObject();
    QTimer::singleShot(
        30, this,
        []
        {
            auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!d)
                return;
            auto *list = d->findChild<QListWidget *>("levelUpChoices");
            if (!list)
            {
                d->reject();
                return;
            }
            for (int i = 0; i < list->count(); ++i)
                list->item(i)->setCheckState(
                    QStringList{"Arcana", "History", "Tool: Thieves’ Tools"}.contains(
                        list->item(i)->text())
                        ? Qt::Checked
                        : Qt::Unchecked);
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    if (!configureFeat(skilled) || !skilled["choices"].toString().contains("Thieves") ||
        skilled["effects"].toArray().size() != 2)
        return fail("Skilled tool checkboxes");
    character = blank();
    character["class"] = "Sorcerer";
    character["primaryLevel"] = 3;
    character["level"] = 3;
    character["manageClassProgression"] = true;
    addClassSpell("Shield", "Sorcerer");
    addClassSpell("Magic Missile", "Sorcerer");
    tasks = {};
    addAdvanceTask(tasks, "Sorcerer", 3, "review",
                   "Review prepared spells and optional spell replacements");
    character["advancementTasks"] = tasks;
    loadWidgets();
    QTimer swap;
    connect(&swap, &QTimer::timeout, this,
            []
            {
                auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!d)
                    return;
                auto *list = d->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                {
                    d->reject();
                    return;
                }
                QString title = d->windowTitle();
                for (int i = 0; i < list->count(); ++i)
                {
                    QString name = list->item(i)->text();
                    bool selected =
                        title.startsWith("Choose prepared")      ? name == "Shield"
                        : title == "Optional spell replacement"  ? name == "Replace one known spell"
                        : title == "Choose the spell to replace" ? name == "Magic Missile"
                                                                 : name == "Mage Armor";
                    list->item(i)->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
                }
                d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    swap.start(10);
    bool swapped = resolveAdvanceTask(0);
    swap.stop();
    if (!swapped)
        return fail("spell replacement checkboxes");
    bool newSpell = false;
    for (auto v : array("spells"))
    {
        auto s = v.toObject();
        if (s["name"] == "Magic Missile")
            return fail("old spell retained after swap");
        if (s["name"] == "Mage Armor")
        {
            newSpell = true;
            if (s["prepared"].toBool())
                return fail("replacement changed preparation count");
        }
    }
    if (!newSpell)
        return fail("replacement spell missing");
    for (auto type : QStringList{"spell", "cantrip", "arcanum"})
    {
        character = blank();
        character["class"] = "Wizard";
        character["primaryLevel"] = 17;
        character["level"] = 17;
        QString known = type == "cantrip" ? "Fire Bolt" : "Magic Missile";
        addClassSpell(known, "Sorcerer", false);
        QJsonArray spellTasks;
        addAdvanceTask(spellTasks, "Wizard", 17, type, "Learn new spell", 1,
                       QJsonObject{{"spellLevel", 1}});
        character["advancementTasks"] = spellTasks;
        loadWidgets();
        bool inspected = false;
        QTimer::singleShot(30, this,
                           [&]
                           {
                               auto *dialog =
                                   qobject_cast<QDialog *>(QApplication::activeModalWidget());
                               if (!dialog)
                                   return;
                               auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                               inspected = list && list->count() > 0;
                               if (list)
                                   for (int i = 0; i < list->count(); ++i)
                                       if (list->item(i)->text() == known)
                                           inspected = false;
                               dialog->reject();
                           });
        auto beforePick = character;
        if (resolveAdvanceTask(0) || !inspected || character != beforePick)
            return fail("known spell visible or cancelled choice changed character: " + type);
    }
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}
bool alreadyKnowsSpell(const QString &name) const
{
    auto normalize = [](QString value)
    { return value.replace(QChar(0x2019), '\'').trimmed().toCaseFolded(); };
    for (auto value : array("spells"))
        if (normalize(value.toObject()["name"].toString()) == normalize(name))
            return true;
    return false;
}
bool alwaysPrepared(const QString &name, const QString &cls) const
{
    for (auto spellName : subclassSpellNames(cls))
        if (normalizedSpell(spellName) == normalizedSpell(name))
            return true;
    if (cls == "Artificer" && normalizedSpell(name) == "mending")
        return true;
    QString normalized = QString(name).replace(QChar(0x2019), '\'').toLower();
    if (cls == "Ranger" && normalized == "hunter's mark")
        return true;
    if (cls == "Paladin" && QStringList{"divine smite", "find steed"}.contains(normalized))
        return true;
    if (cls == "Warlock" && normalized == "contact other plane")
        return true;
    if (cls == "Bard" && classLevel(cls) == 20 &&
        QStringList{"power word heal", "power word kill"}.contains(normalized))
        return true;
    auto choices = character["classChoices"].toObject();
    for (auto key : choices.keys())
        if (key.startsWith(cls + ":Spell Mastery") || key.startsWith(cls + ":Signature Spells"))
            if (jsonStrings(choices[key].toArray()).contains(name) ||
                choices[key].toString() == name)
                return true;
    return false;
}
bool prepareLevelSpells(const QString &cls, int level, QStringList &result)
{
    int maximum =
        isThirdCaster(cls) ? thirdCasterPrepared(level) : classStat(cls, "Prepared spells", level);
    bool wholeList = QStringList{"Cleric", "Druid", "Paladin", "Artificer"}.contains(cls);
    QStringList options, initial;
    QMap<QString, QString> descriptions;
    auto owned = array("spells");
    for (auto v : catalog["spells"].toArray())
    {
        auto spell = v.toObject();
        QString name = spell["name"].toString();
        int spellLevel = spell["level"].toInt();
        bool known = false, prepared = false;
        for (auto entry : owned)
        {
            auto s = entry.toObject();
            if (s["name"].toString() == name &&
                (s["learnedFrom"].toString() == cls ||
                 (s["learnedFrom"].toString().isEmpty() && spellBelongsToClass(spell, cls))))
            {
                known = true;
                prepared |= s["prepared"].toBool();
            }
        }
        if (spellLevel < 1 || spellLevel > classSpellLimit(cls, level) || alwaysPrepared(name, cls))
            continue;
        if (!(wholeList ? spellBelongsToClass(spell, cls) : known))
            continue;
        options << name;
        descriptions[name] = spell["description"].toString();
        if (prepared)
            initial << name;
    }
    if (maximum < 1)
    {
        result << "No prepared-spell selection for this class table.";
        return true;
    }
    QStringList selected;
    if (!chooseAdvanceItems("Choose prepared " + cls +
                                " spells — always-prepared grants are separate",
                            options, 0, selected, descriptions, maximum, initial))
        return false;
    for (int i = 0; i < owned.size(); ++i)
    {
        auto s = owned[i].toObject();
        QString name = s["name"].toString();
        if (options.contains(name) &&
            (s["learnedFrom"].toString() == cls || s["learnedFrom"].toString().isEmpty()))
        {
            s["prepared"] = selected.contains(name);
            owned[i] = s;
        }
    }
    character["spells"] = owned;
    for (auto name : selected)
        addClassSpell(name, cls, true);
    result << "Prepared: " + (selected.isEmpty() ? "None" : selected.join(", "));
    if (QStringList{"Bard", "Sorcerer", "Warlock"}.contains(cls))
    {
        QStringList oldNames;
        for (auto v : array("spells"))
        {
            auto s = v.toObject();
            if (s["learnedFrom"].toString() == cls &&
                findSpell(s["name"].toString())["level"].toInt() > 0 &&
                !alwaysPrepared(s["name"].toString(), cls))
                oldNames << s["name"].toString();
        }
        if (!oldNames.isEmpty())
        {
            QStringList action;
            if (!chooseAdvanceItems("Optional spell replacement",
                                    {"Keep known spells", "Replace one known spell"}, 1, action, {},
                                    -1, {"Keep known spells"}))
                return false;
            if (action[0] == "Replace one known spell")
            {
                QStringList removed, replacement, newNames;
                QMap<QString, QString> rules;
                for (auto name : oldNames)
                    rules[name] = findSpell(name)["description"].toString();
                if (!chooseAdvanceItems("Choose the spell to replace", oldNames, 1, removed, rules))
                    return false;
                for (auto v : catalog["spells"].toArray())
                {
                    auto s = v.toObject();
                    int tier = s["level"].toInt();
                    QString name = s["name"].toString();
                    if (tier > 0 && tier <= classSpellLimit(cls, level) &&
                        spellBelongsToClass(s, cls) && !alreadyKnowsSpell(name) &&
                        !alwaysPrepared(name, cls))
                    {
                        newNames << name;
                        rules[name] = s["description"].toString();
                    }
                }
                if (!chooseAdvanceItems("Choose the replacement spell", newNames, 1, replacement,
                                        rules))
                    return false;
                auto entries = array("spells");
                bool wasPrepared = false;
                for (int i = entries.size() - 1; i >= 0; --i)
                {
                    auto s = entries[i].toObject();
                    if (s["name"].toString() == removed[0] && s["learnedFrom"].toString() == cls)
                    {
                        wasPrepared = s["prepared"].toBool();
                        entries.removeAt(i);
                    }
                }
                character["spells"] = entries;
                addClassSpell(replacement[0], cls, wasPrepared);
                result << "Replaced " + removed[0] + " with " + replacement[0];
            }
        }
    }
    return true;
}
bool chooseClassFeature(QString cls, QString key, QString title, QStringList options,
                        const QString &description, QStringList &result)
{
    auto choices = character["classChoices"].toObject();
    QMap<QString, QString> descriptions;
    for (auto option : options)
        descriptions[option] = description;
    QStringList selected;
    if (!chooseAdvanceItems(title, options, 1, selected, descriptions, -1,
                            {choices[key].toString()}))
        return false;
    choices[key] = selected[0];
    character["classChoices"] = choices;
    result << title + ": " + selected[0];
    Q_UNUSED(cls);
    return true;
}
bool reviewLevelFeatures(const QString &cls, int level, const QString &title, QStringList &result)
{
    QString feature = classStep(cls, level)["features"].toString();
    if (cls == "Cleric" && level == 7 &&
        !chooseClassFeature(cls, "Cleric:Blessed Strikes", "Choose Blessed Strikes",
                            {"Divine Strike", "Potent Spellcasting"}, featureReference(cls, 7),
                            result))
        return false;
    if (cls == "Druid" && level == 7 &&
        !chooseClassFeature(cls, "Druid:Elemental Fury", "Choose Elemental Fury",
                            {"Potent Spellcasting", "Primal Strike"}, featureReference(cls, 7),
                            result))
        return false;
    QString subclass;
    for (auto v : classRoster())
        if (v.toObject()["name"].toString() == cls)
            subclass = v.toObject()["subclass"].toString();
    if (cls == "Ranger" && subclass == "Hunter" && level == 3 &&
        !chooseClassFeature(cls, "Ranger:Hunter's Prey", "Choose Hunter's Prey",
                            {"Colossus Slayer", "Horde Breaker"},
                            "Colossus Slayer: once per turn, a weapon hit against an injured "
                            "creature can deal an extra 1d8. Horde Breaker: once per turn, a "
                            "weapon attack can grant another attack against a different nearby "
                            "target. Resolve the conditional attack or damage when used.",
                            result))
        return false;
    if (cls == "Ranger" && subclass == "Hunter" && level == 7 &&
        !chooseClassFeature(cls, "Ranger:Defensive Tactics", "Choose Defensive Tactics",
                            {"Escape the Horde", "Multiattack Defense"},
                            "Escape the Horde: Opportunity Attacks against you have disadvantage. "
                            "Multiattack Defense: after a creature hits you, its other attacks "
                            "against you have disadvantage for the rest of that turn.",
                            result))
        return false;
    int mastery =
        title.startsWith("Review new class") ? classStat(cls, "Weapon mastery", level) : 0;
    if (title.startsWith("Review new class") &&
        QStringList{"Paladin", "Ranger", "Rogue"}.contains(cls))
        mastery = 2;
    if (mastery > jsonStrings(character["classMasteries"].toObject()[cls].toArray()).size())
    {
        QStringList options;
        QMap<QString, QString> descriptions;
        for (auto v : catalog["weapons"].toArray())
        {
            auto w = v.toObject();
            if (!classWeaponTrained(w))
                continue;
            QString name = w["name"].toString();
            options << name;
            descriptions[name] = w["dice"].toString() + " " + w["type"].toString() + "\n" +
                                 w["properties"].toString() +
                                 "\nMastery: " + w["mastery"].toString() + "\n\n" +
                                 masteryDescription(w["mastery"].toString());
        }
        auto masteries = character["classMasteries"].toObject();
        QStringList selected;
        if (!chooseNewAdvanceItems("Choose " + cls + " weapon masteries", options, mastery,
                                   selected, descriptions, jsonStrings(masteries[cls].toArray())))
            return false;
        masteries[cls] = QJsonArray::fromStringList(selected);
        character["classMasteries"] = masteries;
        result << "Weapon masteries: " + selected.join(", ");
    }
    if (cls == "Wizard" && (level == 18 || level == 20))
    {
        auto choices = character["classChoices"].toObject();
        for (int tier : level == 18 ? QList<int>{1, 2} : QList<int>{3})
        {
            QStringList options;
            QMap<QString, QString> descriptions;
            for (auto v : array("spells"))
            {
                auto s = v.toObject();
                auto ref = findSpell(s["name"].toString());
                if (ref["level"].toInt() != tier || !ref["meta"].toString().contains("Wizard"))
                    continue;
                if (level == 18 &&
                    !ref["fields"].toObject()["Casting Time"].toString().startsWith("Action"))
                    continue;
                QString name = s["name"].toString();
                options << name;
                descriptions[name] = ref["description"].toString();
            }
            options.removeDuplicates();
            QString key = level == 18 ? "Wizard:Spell Mastery " + QString::number(tier)
                                      : "Wizard:Signature Spells";
            QStringList selected;
            if (!chooseAdvanceItems(key, options, level == 18 ? 1 : 2, selected, descriptions, -1,
                                    jsonStrings(choices[key].toArray())))
                return false;
            choices[key] = QJsonArray::fromStringList(selected);
            character["classChoices"] = choices;
            for (auto name : selected)
                addClassSpell(name, cls, true);
            result << key + ": " + selected.join(", ");
        }
    }
    QStringList confirmations;
    if (feature != "—")
        for (auto name : feature.split(", ", Qt::SkipEmptyParts))
            confirmations << "Reviewed: " + name;
    QMap<QString, QString> descriptions;
    for (auto value : advancementCatalog[cls].toObject()["features"].toArray())
    {
        auto f = value.toObject();
        if (f["level"].toInt() <= level)
            descriptions["Reviewed: " + f["name"].toString()] = f["name"].toString() + "\n\n" +
                                                                f["description"].toString() +
                                                                "\n\nSource: SRD 5.2.1 — " + cls;
    }
    for (auto name : confirmations)
        if (descriptions.value(name).isEmpty())
        {
            QString featureName = name.mid(QString("Reviewed: ").size());
            auto feat = featReference(featureName);
            descriptions[name] = feat["description"].toString();
            if (descriptions[name].isEmpty())
                descriptions[name] = featureReference(cls, level);
        }
    if (confirmations.isEmpty())
        return true;
    QStringList checked;
    if (!chooseAdvanceItems(title, confirmations, confirmations.size(), checked, descriptions))
        return false;
    result.append(checked);
    return true;
}
