bool selfTestProgression()
{
    auto original = character;
    bool originalDirty = dirty;
    character = blank();
    loadWidgets();
    if (speciesCatalog.size() != 34 || character["abilitiesLocked"].toBool())
        return false;
    character["species"] = "Half-Elf";
    character["speciesProfile"] = QJsonObject{{"name", "Half-Elf"},
                                              {"enabled", true},
                                              {"useSpeed", true},
                                              {"skill", "Perception"},
                                              {"skill2", "Persuasion"}};
    if (effectiveSpeed() != 30 || selectedSpecies()["darkvision"].toInt() != 60 ||
        speciesSkillCount() != 2 || effectAdvantage({"charmSave"}) != 1 ||
        effectTraining({"skill:Perception"}, 0) != 1 ||
        effectTraining({"skill:Persuasion"}, 0) != 1 || number("Strength") != 15 ||
        effectBonus({"ability:Charisma"}) != 0)
        return false;
    character["species"] = "Dwarf";
    character["speciesProfile"] =
        QJsonObject{{"name", "Dwarf"}, {"enabled", true}, {"useSpeed", true}};
    if (maximumHp() != 12 || number("Strength") != 15)
        return false;
    character["level"] = 5;
    if (maximumHp() != 16)
        return false;
    character["species"] = "Elf";
    character["speciesProfile"] = QJsonObject{{"name", "Elf"},
                                              {"enabled", true},
                                              {"useSpeed", true},
                                              {"lineage", "Wood Elf"},
                                              {"skill", "Perception"}};
    if (effectiveSpeed() != 35 || effectTraining({"skill:Perception"}, 0) != 1)
        return false;
    character["species"] = "Gnome";
    character["speciesProfile"] = QJsonObject{{"name", "Gnome"}, {"enabled", true}};
    if (effectAdvantage({"save:Intelligence"}) != 1 || effectAdvantage({"save:Constitution"}) != 0)
        return false;
    for (auto entry : speciesCatalog)
    {
        auto definition = entry.toObject();
        character = blank();
        character["species"] = definition["name"];
        character["speciesProfile"] =
            QJsonObject{{"name", definition["name"]}, {"enabled", true}, {"useSpeed", true}};
        loadWidgets();
        if (effectiveSpeed() != definition["speed"].toInt())
            return false;
        if (definition["description"].toString().isEmpty())
            return false;
    }
    character = blank();
    character["species"] = "Tortle";
    character["speciesProfile"] =
        QJsonObject{{"name", "Tortle"}, {"enabled", true}, {"useSpeed", true}};
    if (armorClass() != 17)
        return false;
    character["Dexterity"] = 20;
    if (armorClass() != 17)
        return false;
    character["species"] = "Lizardfolk";
    character["speciesProfile"] = QJsonObject{{"name", "Lizardfolk"}, {"enabled", true}};
    if (speciesNaturalAc() != 18)
        return false;
    character["wearingArmor"] = true;
    if (speciesNaturalAc() != 0)
        return false;
    character["wearingArmor"] = false;
    character["species"] = "Warforged";
    character["speciesProfile"] = QJsonObject{{"name", "Warforged"}, {"enabled", true}};
    if (effectBonus({"ac"}) != 1)
        return false;
    character["species"] = "Human";
    if (effectBonus({"ac"}) != 0)
        return false;
    character["species"] = "Kobold";
    character["speciesProfile"] =
        QJsonObject{{"name", "Kobold"}, {"enabled", true}, {"lineage", "Defiance"}};
    if (effectAdvantage({"fearSave"}) != 1 || speciesSkillCount() != 0)
        return false;
    character["speciesProfile"] =
        QJsonObject{{"name", "Kobold"}, {"enabled", true}, {"lineage", "Craftiness"}};
    if (effectAdvantage({"fearSave"}) != 0 || speciesSkillCount() != 1)
        return false;
    character["species"] = "Shifter";
    character["speciesProfile"] = QJsonObject{};
    loadWidgets();
    bool described = false;
    QTimer::singleShot(30, this,
                       [&]
                       {
                           auto *dialog =
                               qobject_cast<QDialog *>(QApplication::activeModalWidget());
                           if (!dialog)
                               return;
                           auto *lineage = dialog->findChild<QComboBox *>("speciesLineage");
                           auto *details = dialog->findChild<QPlainTextEdit *>();
                           if (lineage)
                               lineage->setCurrentText("Swiftstride");
                           described = details &&
                                       details->toPlainText().contains("+10 walking speed") &&
                                       !details->toPlainText().contains("Source:");
                           dialog->grab().save(QCoreApplication::applicationDirPath() +
                                               "/preview-expanded-species.png");
                           dialog->reject();
                       });
    auto beforeSpeciesDialog = character;
    configureSpecies();
    if (!described || character != beforeSpeciesDialog)
        return false;
    character = blank();
    loadWidgets();
    dirty = true;
    if (!save() || character["abilitiesLocked"].toBool())
        return false;
    if (!lockAbilities() || !character["abilitiesLocked"].toBool())
        return false;
    setNumber("Strength", 20);
    if (number("Strength") != 15 || qobject_cast<QSpinBox *>(fields["Strength"])->value() != 15)
        return false;
    character["abilitiesLocked"] = false;
    character["level"] = 2;
    character["levelUpDraft"] = true;
    character["levelUpConModifier"] = 1;
    setNumber("Constitution", 15);
    if (maximumHp() != 13 || !lockAbilities() || number("hpMax") != 13 || maximumHp() != 13)
        return false;
    int previousLevel = number("level"), previousHp = number("hpMax");
    QString error;
    if (!applyLevelGain("Fighter", 6, error))
        return false;
    if (number("level") != previousLevel + 1 || number("hpMax") != previousHp + 8 ||
        character["abilitiesLocked"].toBool())
        return false;
    auto tasks = array("advancementTasks");
    for (int i = 0; i < tasks.size(); ++i)
    {
        auto task = tasks[i].toObject();
        task["done"] = true;
        tasks[i] = task;
    }
    character["advancementTasks"] = tasks;
    if (!lockAbilities())
        return false;
    QFile persisted(path());
    if (!persisted.open(QIODevice::ReadOnly) ||
        !QJsonDocument::fromJson(persisted.readAll()).object()["abilitiesLocked"].toBool())
        return false;
    character = original;
    loadWidgets();
    dirty = originalDirty;
    return true;
}
QJsonObject selectedSpecies() const
{
    for (auto entry : speciesCatalog)
        if (entry.toObject()["name"] == character["species"])
            return entry.toObject();
    return {};
}
QJsonObject selectedSpeciesVariant() const
{
    auto profile = character["speciesProfile"].toObject();
    if (profile["name"] != character["species"])
        return {};
    for (auto value : selectedSpecies()["variants"].toArray())
        if (value.toObject()["name"] == profile["lineage"])
            return value.toObject();
    return {};
}
int speciesNaturalAc() const
{
    auto d = speciesDefinition();
    if (!d.contains("naturalAc"))
        return 0;
    if (!d["ignoreArmor"].toBool() &&
        (!wornCatalogArmor().isEmpty() || character["wearingArmor"].toBool()))
        return 0;
    return d["naturalAc"].toInt() + (d["naturalDex"].toBool() ? ability(1) : 0);
}
QJsonObject speciesDefinition() const
{
    auto profile = character["speciesProfile"].toObject();
    if (!profile["enabled"].toBool() || profile["name"] != character["species"])
        return {};
    for (auto v : speciesCatalog)
        if (v.toObject()["name"] == character["species"])
            return v.toObject();
    return {};
}
QJsonObject speciesVariant() const
{
    for (auto v : speciesDefinition()["variants"].toArray())
        if (v.toObject()["name"] == character["speciesProfile"].toObject()["lineage"])
            return v.toObject();
    return {};
}
QJsonArray speciesEffects() const
{
    auto definition = speciesDefinition();
    auto result = definition["effects"].toArray();
    for (auto effect : speciesVariant()["effects"].toArray())
        result.append(effect);
    auto profile = character["speciesProfile"].toObject();
    QStringList skills = jsonStrings(definition["fixedSkills"].toArray());
    for (auto key : {"skill", "skill2"})
        if (!profile[key].toString().isEmpty())
            skills << profile[key].toString();
    skills.removeDuplicates();
    for (auto skill : skills)
        if (!definition.isEmpty() && skillNames.contains(skill))
            result.append(
                QJsonObject{{"target", "skill:" + skill}, {"op", "proficiency"}, {"value", 1}});
    return result;
}
int speciesSpeed() const
{
    auto d = speciesDefinition(), v = speciesVariant();
    if (d.isEmpty() || !character["speciesProfile"].toObject()["useSpeed"].toBool())
        return number("speed");
    return v["speed"].toInt(d["speed"].toInt(30));
}
void configureSpecies()
{
    QJsonObject definition;
    for (auto v : speciesCatalog)
        if (v.toObject()["name"] == character["species"])
            definition = v.toObject();
    if (definition.isEmpty())
    {
        displayError(
            "Choose a listed species first. Custom species traits can be entered in Feats & "
            "bonuses.");
        return;
    }
    auto old = character["speciesProfile"].toObject();
    bool same = old["name"] == character["species"];
    QDialog dialog(this);
    dialog.setWindowTitle(text("species") + " — species traits");
    dialog.resize(650, 650);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *enable = new QCheckBox("Apply fixed species bonuses");
    enable->setChecked(!same || old["enabled"].toBool());
    form->addRow(enable);
    auto *speed = new QCheckBox("Use species walking speed");
    speed->setChecked(!same || old["useSpeed"].toBool());
    form->addRow(speed);
    auto *lineage = new QComboBox;
    lineage->setObjectName("speciesLineage");
    for (auto v : definition["variants"].toArray())
        lineage->addItem(v.toObject()["name"].toString());
    if (same && lineage->findText(old["lineage"].toString()) >= 0)
        lineage->setCurrentText(old["lineage"].toString());
    form->addRow("Lineage / ancestry", lineage);
    lineage->setEnabled(lineage->count() > 0);
    auto *skill = new QComboBox;
    skill->addItem("Choose later / already included");
    auto choices = definition["skillChoices"];
    if (choices.toString() == "all")
        skill->addItems(skillNames);
    else
        for (auto v : choices.toArray())
            skill->addItem(v.toString());
    if (same && skill->findText(old["skill"].toString()) >= 0)
        skill->setCurrentText(old["skill"].toString());
    skill->setEnabled(skill->count() > 1);
    form->addRow("Species skill proficiency", skill);
    layout->addLayout(form);
    auto *skill2 = new QComboBox;
    form->addRow("Second species skill", skill2);
    auto updateSkills = [=]
    {
        QJsonObject variant;
        for (auto value : definition["variants"].toArray())
            if (value.toObject()["name"].toString() == lineage->currentText())
                variant = value.toObject();
        auto choices =
            variant.contains("skillChoices") ? variant["skillChoices"] : definition["skillChoices"];
        QStringList allowed =
            choices.toString() == "all" ? skillNames : jsonStrings(choices.toArray());
        QString current = skill->currentText(), current2 = skill2->currentText();
        QSignalBlocker b(skill), b2(skill2);
        skill->clear();
        skill->addItem("Choose later / already included");
        skill->addItems(allowed);
        skill2->clear();
        skill2->addItem("Choose later / already included");
        skill2->addItems(allowed);
        if (skill->findText(current) >= 0)
            skill->setCurrentText(current);
        if (skill2->findText(current2) >= 0)
            skill2->setCurrentText(current2);
        skill->setEnabled(!allowed.isEmpty());
        skill2->setEnabled(!allowed.isEmpty() && definition["skillCount"].toInt(1) == 2);
    };
    connect(lineage, &QComboBox::currentTextChanged, &dialog, updateSkills);
    updateSkills();
    if (same)
    {
        skill->setCurrentText(old["skill"].toString());
        skill2->setCurrentText(old["skill2"].toString());
    }
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    auto describe = [=]
    {
        QJsonObject variant;
        for (auto v : definition["variants"].toArray())
            if (v.toObject()["name"].toString() == lineage->currentText())
                variant = v.toObject();
        QStringList res;
        for (auto a : {definition["resistances"].toArray(), variant["resistances"].toArray()})
            for (auto v : a)
                res << v.toString();
        QStringList spells;
        for (auto a : {definition["spells"].toArray(), variant["spells"].toArray()})
            for (auto v : a)
                spells << QString("%1 (character level %2)")
                              .arg(v.toObject()["name"].toString())
                              .arg(v.toObject()["level"].toInt(1));
        QStringList fixedSkills = jsonStrings(definition["fixedSkills"].toArray());
        QString extra =
            "\n\n" + sourcesLast(definition["rules"].toString()) +
            "\nCreature type: " + definition["creatureType"].toString("Humanoid") +
            "\nSize choices: " + jsonStrings(definition["sizes"].toArray()).join(", ") +
            "\nOther movement: " + definition["movement"].toString("None") +
            "\nFixed skills: " + (fixedSkills.isEmpty() ? QString("None") : fixedSkills.join(", "));
        details->setPlainText(
            QString("2024 rules: ability increases come from backgrounds.\n\nSpeed: %1 "
                    "ft\nDarkvision: %2 ft\nResistances: %3\n\n%4\n\n%5\n\nGranted spells: "
                    "%6\n\nFixed HP, walking speed, skills, AC and general saving-throw bonuses "
                    "apply automatically when enabled. Condition-specific saves, "
                    "resistances, "
                    "limited-use powers and spells are references: apply damage resistance before "
                    "entering damage, add granted spells to your spellbook, and track uses "
                    "manually. Human: choose the extra Origin feat in Feats & bonuses. Avoid "
                    "adding a bonus twice if it was already included in your base values.")
                .arg(variant["speed"].toInt(definition["speed"].toInt(30)))
                .arg(variant["darkvision"].toInt(definition["darkvision"].toInt()))
                .arg(res.isEmpty() ? "None" : res.join(", "), definition["description"].toString(),
                     variant["description"].toString(),
                     spells.isEmpty() ? "None" : spells.join(", ")) +
            extra);
    };
    connect(lineage, &QComboBox::currentTextChanged, &dialog, describe);
    describe();
    layout->addWidget(details, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (skill->currentIndex() > 0 && skill2->isEnabled() && skill2->currentIndex() > 0 &&
        skill->currentText() == skill2->currentText())
    {
        displayError("Choose two different species skills. Your previous choices were preserved.");
        return;
    }
    put("speciesProfile",
        QJsonObject{{"name", text("species")},
                    {"enabled", enable->isChecked()},
                    {"useSpeed", speed->isChecked()},
                    {"lineage", lineage->currentText()},
                    {"skill", skill->isEnabled() && skill->currentIndex() > 0 ? skill->currentText()
                                                                              : QString()},
                    {"skill2", skill2->isEnabled() && skill2->currentIndex() > 0
                                   ? skill2->currentText()
                                   : QString()}});
}
void updateAbilityLock()
{
    if (!abilityLockState)
        return;
    bool locked = character["abilitiesLocked"].toBool();
    abilityLockState->setText(locked
                                  ? "Base abilities saved and locked. Use Level up to change them."
                                  : "Ability setup is open. Save abilities & lock when finished.");
    for (auto key : abilities)
        if (auto *w = qobject_cast<QSpinBox *>(fields.value(key)))
        {
            w->setReadOnly(baseAbilitiesProtected());
            w->setButtonSymbols(baseAbilitiesProtected() ? QAbstractSpinBox::NoButtons
                                                         : QAbstractSpinBox::UpDownArrows);
        }
    for (auto *w : abilitySetupButtons)
        w->setEnabled(!locked && !character["levelUpDraft"].toBool());
}
bool lockAbilities()
{
    if (!abilityBudgetProblem().isEmpty() && !assignAbilityPool())
    {
        bool saved = save();
        if (saved)
            saveState->setText(
                "Draft saved — assign the available ability scores before finalizing");
        return saved;
    }

    if (!creationProblem().isEmpty() && !configureStartingProficiencies())
    {
        bool saved = save();
        if (saved)
            saveState->setText(
                "Draft saved — starting proficiencies are required before finalizing");
        return saved;
    }
    if (!pendingAdvanceNames().isEmpty())
    {
        bool saved = save();
        showAdvancement();
        if (saved)
            saveState->setText("Draft saved — complete level-up choices to lock abilities");
        return saved;
    }
    auto before = character;
    if (character["levelUpDraft"].toBool())
    {
        character["hpMax"] =
            number("hpMax") +
            number("level", 1) * (modifier(number("Constitution")) - number("levelUpConModifier"));
        character["levelUpDraft"] = false;
    }
    character["abilitiesLocked"] = true;
    dirty = true;
    if (!save())
    {
        character = before;
        dirty = true;
        return false;
    }
    loadWidgets();
    return true;
}
void levelUp()
{
    guidedLevelUp();
}
