bool selfTestCreation()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](QString reason)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Creation: " + reason).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    auto fixture = [&](QString cls, QString species = "Human", QString bg = "Soldier")
    {
        character = blank();
        character["creationRequired"] = true;
        character["class"] = cls;
        character["species"] = species;
        character["background"] = bg;
        character["speciesProfile"] =
            QJsonObject{{"name", species}, {"enabled", true}, {"useSpeed", true}};
        auto profile = character["speciesProfile"].toObject();
        if (!selectedSpecies()["variants"].toArray().isEmpty())
            profile["lineage"] = selectedSpecies()["variants"].toArray()[0].toObject()["name"];
        character["speciesProfile"] = profile;
        auto fixedSpecies = jsonStrings(selectedSpecies()["fixedSkills"].toArray());
        auto background = backgroundSkills(bg);
        QStringList chosen;
        for (auto skill : startingSkills(cls))
            if (!background.contains(skill) && !fixedSpecies.contains(skill) &&
                chosen.size() < startingSkillCount(cls))
                chosen << skill;
        QString racial;
        for (auto skill : speciesSkills())
            if (!background.contains(skill) && !chosen.contains(skill))
            {
                racial = skill;
                break;
            }
        QString racial2;
        if (speciesSkillCount() == 2)
            for (auto skill : speciesSkills())
                if (!background.contains(skill) && !chosen.contains(skill) && skill != racial)
                {
                    racial2 = skill;
                    break;
                }
        QJsonArray expertise;
        if (cls == "Rogue")
        {
            expertise.append(chosen[0]);
            expertise.append(chosen[1]);
        }
        character["creationTraining"] =
            QJsonObject{{"class", cls},
                        {"background", bg},
                        {"species", species},
                        {"classSkills", QJsonArray::fromStringList(chosen)},
                        {"backgroundSkills", QJsonArray::fromStringList(background)},
                        {"speciesSkill", racial},
                        {"speciesSkill2", racial2},
                        {"expertise", expertise}};
        loadWidgets();
    };
    for (auto value : speciesCatalog)
    {
        QString name = value.toObject()["name"].toString();
        fixture("Fighter", name, "Sage");
        if (!creationProblem().isEmpty())
            return fail(name + ": " + creationProblem());
        for (auto skill : speciesCreationSkills(character["creationTraining"].toObject()))
            if (effectTraining({"skill:" + skill}, 0) < 1)
                return fail(name + " missing skill " + skill);
        if (speciesSkillCount() == 2)
        {
            auto complete = character["creationTraining"].toObject();
            auto incomplete = complete;
            incomplete["speciesSkill2"] = "";
            character["creationTraining"] = incomplete;
            if (creationProblem().isEmpty())
                return fail(name + " accepted missing second skill");
            incomplete["speciesSkill2"] = complete["speciesSkill"];
            character["creationTraining"] = incomplete;
            if (creationProblem().isEmpty())
                return fail(name + " accepted duplicate species skills");
        }
    }
    fixture("Fighter", "Tabaxi", "Sailor");
    if (speciesSkillCount() != 1 || !creationProblem().isEmpty())
        return fail("duplicate fixed/background skill must grant replacement choice");
    fixture("Fighter", "Kenku", "Sage");
    bool twoChoices = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *second = dialog->findChild<QComboBox *>("secondStartingSpeciesSkill");
            twoChoices = second && second->isEnabled() && second->count() == skillNames.size() + 1;
            dialog->grab().save(QCoreApplication::applicationDirPath() +
                                "/preview-expanded-species-skills.png");
            dialog->reject();
        });
    if (configureStartingProficiencies() || !twoChoices)
        return fail("two species choices not shown");
    for (auto cls : classes)
    {
        fixture(cls);
        if (!creationProblem().isEmpty())
            return fail(cls + ": " + creationProblem());
        int count = 0;
        for (int i = 0; i < 18; ++i)
            if (effectTraining({"skill:*", "skill:" + skillNames[i]}, 0) > 0)
                ++count;
        if (count != startingSkillCount(cls) + 3)
            return fail("wrong number of unique grants");
        auto c = character["creationTraining"].toObject();
        auto chosen = c["classSkills"].toArray();
        chosen.removeLast();
        c["classSkills"] = chosen;
        character["creationTraining"] = c;
        if (creationProblem().isEmpty())
            return fail("missing class skill accepted");
    }
    fixture("Rogue", "Elf", "Soldier");
    if (!creationProblem().isEmpty())
        return fail("Elf Rogue selection");
    auto c = character["creationTraining"].toObject();
    auto expertise = c["expertise"].toArray();
    if (effectTraining({"skill:" + expertise[0].toString()}, 0) != 2)
        return fail("Rogue expertise not applied");
    QJsonArray manualSkills;
    for (int i = 0; i < 18; ++i)
        manualSkills.append(i == 2 ? 1 : 0);
    character["skillProfs"] = manualSkills;
    c["expertise"] = QJsonArray{"Arcana", expertise[1]};
    character["creationTraining"] = c;
    if (!creationProblem().isEmpty())
        return fail("existing proficiency cannot receive Rogue expertise");
    c["speciesSkill"] = "Arcana";
    character["creationTraining"] = c;
    if (creationProblem().isEmpty())
        return fail("invalid Elf skill accepted");
    fixture("Fighter", "Dwarf");
    if (!creationProblem().isEmpty())
        return fail("Dwarf incorrectly requires skill");
    fixture("Fighter");
    character["background"] = "Sage";
    if (creationProblem().isEmpty() || effectTraining({"skill:Athletics"}, 0) != 0 ||
        effectTraining({"skill:Arcana"}, 0) != 1)
        return fail("background change left stale grants");
    fixture("Fighter");
    auto before = character;
    QTimer::singleShot(20, this,
                       []
                       {
                           if (auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                               d->reject();
                       });
    if (configureStartingProficiencies() || character != before)
        return fail("cancel changed grants");
    character = blank();
    character["creationRequired"] = true;
    loadWidgets();
    bool picked = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!d)
                return;
            auto *list = d->findChild<QListWidget *>("startingClassSkills");
            if (!list)
            {
                d->reject();
                return;
            }
            if (auto *box = d->findChild<QComboBox *>("firstStartingSpeciesSkill"))
                box->setCurrentText("Perception");
            int count = 0;
            for (int i = 0; i < list->count() && count < 2; ++i)
                if (list->item(i)->flags() & Qt::ItemIsEnabled)
                {
                    list->item(i)->setCheckState(Qt::Checked);
                    ++count;
                }
            d->grab().save(QCoreApplication::applicationDirPath() + "/preview-creation.png");
            picked = true;
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
    if (!configureStartingProficiencies() || !picked || !creationProblem().isEmpty())
        return fail("required creation picker");
    if (!lockAbilities() || !character["abilitiesLocked"].toBool())
        return fail("completed creation cannot finalize");
    QFile persisted(path());
    if (!persisted.open(QIODevice::ReadOnly) ||
        !QJsonDocument::fromJson(persisted.readAll()).object()["creationRequired"].toBool())
        return fail("creation persistence");
    auto profile = character["creationTraining"].toObject();
    auto chosen = profile["classSkills"].toArray();
    chosen.removeLast();
    profile["classSkills"] = chosen;
    character["creationTraining"] = profile;
    character["abilitiesLocked"] = false;
    QTimer::singleShot(20, this,
                       []
                       {
                           if (auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                               d->reject();
                       });
    if (!lockAbilities() || character["abilitiesLocked"].toBool())
        return fail("unfinished creation finalized");
    QString error;
    if (applyLevelGain("Fighter", 6, error))
        return fail("unfinished creation leveled up");
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}
QStringList startingSkills(const QString &cls) const
{
    static const QMap<QString, QStringList> options{
        {"Artificer",
         {"Arcana", "History", "Investigation", "Medicine", "Nature", "Perception",
          "Sleight of Hand"}},
        {"Barbarian",
         {"Animal Handling", "Athletics", "Intimidation", "Nature", "Perception", "Survival"}},
        {"Bard", skillNames},
        {"Cleric", {"History", "Insight", "Medicine", "Persuasion", "Religion"}},
        {"Druid",
         {"Animal Handling", "Arcana", "Insight", "Medicine", "Nature", "Perception", "Religion",
          "Survival"}},
        {"Fighter",
         {"Acrobatics", "Animal Handling", "Athletics", "History", "Insight", "Intimidation",
          "Persuasion", "Perception", "Survival"}},
        {"Monk", {"Acrobatics", "Athletics", "History", "Insight", "Religion", "Stealth"}},
        {"Paladin", {"Athletics", "Insight", "Intimidation", "Medicine", "Persuasion", "Religion"}},
        {"Ranger",
         {"Animal Handling", "Athletics", "Insight", "Investigation", "Nature", "Perception",
          "Stealth", "Survival"}},
        {"Rogue",
         {"Acrobatics", "Athletics", "Deception", "Insight", "Intimidation", "Investigation",
          "Perception", "Persuasion", "Sleight of Hand", "Stealth"}},
        {"Sorcerer", {"Arcana", "Deception", "Insight", "Intimidation", "Persuasion", "Religion"}},
        {"Warlock",
         {"Arcana", "Deception", "History", "Intimidation", "Investigation", "Nature", "Religion"}},
        {"Wizard",
         {"Arcana", "History", "Insight", "Investigation", "Medicine", "Nature", "Religion"}}};
    return options.value(cls);
}
int startingSkillCount(const QString &cls) const
{
    return cls == "Bard" || cls == "Ranger" ? 3 : cls == "Rogue" ? 4 : 2;
}
bool selfTestSkillTrainingDisplay()
{
    const auto snapshot = character;
    auto finish = [&](bool okay)
    {
        character = snapshot;
        loadWidgets();
        if (!okay)
        {
            QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (f.open(QIODevice::WriteOnly))
                f.write("Skill training display regression");
        }
        return okay;
    };
    character = blank();
    character["creationRequired"] = false;
    character["creationTraining"] = QJsonObject{};
    character["effectSources"] = QJsonArray{
        QJsonObject{{"name", "Test grants"},
                    {"active", true},
                    {"effects", QJsonArray{effect("skill:Arcana", 0, "flat", "proficiency"),
                                           effect("skill:Stealth", 0, "flat", "expertise")}}}};
    loadWidgets();
    const int arcana = skillNames.indexOf("Arcana"), stealth = skillNames.indexOf("Stealth");
    for (auto *box : skillProf)
        if (box->count() != 3 || box->itemText(1) != "Proficient")
            return finish(false);
    if (skillProf[arcana]->currentIndex() != 1 || skillProf[stealth]->currentIndex() != 2 ||
        array("skillProfs").at(arcana).toInt() != 0)
        return finish(false);
    skillProf[0]->setCurrentIndex(1);
    if (array("skillProfs").at(0).toInt() != 1 || array("skillProfs").at(arcana).toInt() != 0 ||
        array("skillProfs").at(stealth).toInt() != 0)
        return finish(false);
    const int expected = ability(skillAbilities[arcana]) + pb();
    if (skillBonus(arcana) != expected)
        return finish(false);
    skillProf[arcana]->setCurrentIndex(0);
    if (skillProf[arcana]->currentIndex() != 1 || skillBonus(arcana) != expected)
        return finish(false);
    put("effectSources", QJsonArray{});
    if (skillProf[arcana]->currentIndex() != 0 || skillProf[stealth]->currentIndex() != 0 ||
        skillProf[0]->currentIndex() != 1)
        return finish(false);
    loadWidgets();
    if (skillProf[0]->currentIndex() != 1 || skillProf[arcana]->currentIndex() != 0)
        return finish(false);
    return finish(true);
}
QStringList backgroundSkills(const QString &name) const
{
    return jsonStrings(backgroundReference(name)["skills"].toArray());
}
QStringList speciesSkills() const
{
    auto definition = selectedSpecies(), variant = selectedSpeciesVariant();
    QJsonValue choices =
        variant.contains("skillChoices") ? variant["skillChoices"] : definition["skillChoices"];
    for (auto skill : jsonStrings(definition["fixedSkills"].toArray()))
        if (backgroundSkills(text("background")).contains(skill))
            return skillNames;
    return choices.toString() == "all" ? skillNames : jsonStrings(choices.toArray());
}
int speciesSkillCount() const
{
    auto definition = selectedSpecies(), variant = selectedSpeciesVariant();
    QJsonValue choices =
        variant.contains("skillChoices") ? variant["skillChoices"] : definition["skillChoices"];
    int count = choices.toString() == "all" || !choices.toArray().isEmpty()
                    ? definition["skillCount"].toInt(1)
                    : 0;
    for (auto skill : jsonStrings(definition["fixedSkills"].toArray()))
        if (backgroundSkills(text("background")).contains(skill))
            ++count;
    return count;
}
QStringList speciesCreationSkills(QJsonObject c) const
{
    QStringList skills = jsonStrings(selectedSpecies()["fixedSkills"].toArray());
    if (!c["speciesSkill"].toString().isEmpty())
        skills << c["speciesSkill"].toString();
    if (speciesSkillCount() == 2 && !c["speciesSkill2"].toString().isEmpty())
        skills << c["speciesSkill2"].toString();
    skills.removeDuplicates();
    return skills;
}

QStringList jsonStrings(QJsonArray a) const
{
    QStringList out;
    for (auto v : a)
        out << v.toString();
    return out;
}
bool matchingCreation() const
{
    auto c = character["creationTraining"].toObject();
    return c["class"] == character["class"] && c["background"] == character["background"] &&
           c["species"] == character["species"];
}
QStringList extraCreationSkills() const
{
    QStringList out;
    auto effects = activeEffects();
    for (int i = 0; i < skillNames.size(); ++i)
    {
        bool trained = array("skillProfs").at(i).toInt() > 0;
        for (auto v : effects)
        {
            auto e = v.toObject();
            QString source = e["source"].toString();
            if (source == "Starting proficiencies" || source.startsWith("Species: "))
                continue;
            if ((e["target"] == "skill:*" || e["target"].toString() == "skill:" + skillNames[i]) &&
                QStringList{"proficiency", "expertise", "upgrade"}.contains(e["op"].toString()))
                trained = true;
        }
        if (trained)
            out << skillNames[i];
    }
    return out;
}
QString creationProblem() const
{
    if (!character["creationRequired"].toBool())
        return {};
    if (!matchingCreation())
        return "Choose the required starting proficiencies for this class, background and species.";
    auto c = character["creationTraining"].toObject();
    auto chosen = jsonStrings(c["classSkills"].toArray()),
         background = backgroundSkills(text("background"));
    if (background.isEmpty())
        background = jsonStrings(c["backgroundSkills"].toArray());
    if (background.size() != 2 || QSet<QString>(background.begin(), background.end()).size() != 2)
        return "Choose two different background skills.";
    for (auto s : background)
        if (!skillNames.contains(s) || !allowedBackgroundSkills().contains(s))
            return "Choose background skills from the allowed list.";
    for (auto fixed :
         jsonStrings(backgroundReference(text("background"))["requiredSkills"].toArray()))
        if (!background.contains(fixed))
            return "This background requires " + fixed + ".";
    if (chosen.size() != startingSkillCount(text("class")) ||
        QSet<QString>(chosen.begin(), chosen.end()).size() != chosen.size())
        return "Choose exactly " + QString::number(startingSkillCount(text("class"))) +
               " different class skills.";
    for (auto s : chosen)
        if (!startingSkills(text("class")).contains(s) || background.contains(s) ||
            jsonStrings(selectedSpecies()["fixedSkills"].toArray()).contains(s))
            return "Choose class skills from the allowed list without duplicating background "
                   "grants.";
    if (selectedSpecies()["expanded"].toBool() &&
        !selectedSpecies()["variants"].toArray().isEmpty() && selectedSpeciesVariant().isEmpty())
        return "Choose a lineage in Species traits & lineage first.";
    auto allowed = speciesSkills();
    QStringList selectedSpeciesSkills;
    for (int i = 0; i < speciesSkillCount(); ++i)
    {
        QString value = c[i == 0 ? "speciesSkill" : "speciesSkill2"].toString();
        if (!allowed.contains(value) || selectedSpeciesSkills.contains(value) ||
            chosen.contains(value) || background.contains(value) ||
            jsonStrings(selectedSpecies()["fixedSkills"].toArray()).contains(value))
            return "Choose " + QString::number(speciesSkillCount()) +
                   " different species skill(s) without duplicating class or background.";
        selectedSpeciesSkills << value;
    }
    if (text("class") == "Rogue")
    {
        auto expertise = jsonStrings(c["expertise"].toArray());
        auto trained = chosen + background + extraCreationSkills();
        trained += speciesCreationSkills(c);
        if (expertise.size() != 2 || expertise[0] == expertise[1])
            return "Rogues must choose two different starting expertise skills.";
        for (auto s : expertise)
            if (!trained.contains(s))
                return "Expertise must use a skill you are proficient in.";
    }
    return {};
}
QJsonArray creationEffects() const
{
    QJsonArray effects;
    if (!character["creationRequired"].toBool() && !matchingCreation())
        return effects;
    auto add = [&](QString skill, QString op = "proficiency")
    {
        if (skillNames.contains(skill))
            effects.append(QJsonObject{{"target", "skill:" + skill}, {"op", op}, {"value", 1}});
    };
    for (auto s : backgroundSkills(text("background")))
        add(s);
    if (matchingCreation())
    {
        auto c = character["creationTraining"].toObject();
        if (backgroundSkills(text("background")).isEmpty())
            for (auto s : jsonStrings(c["backgroundSkills"].toArray()))
                add(s);
        for (auto s : jsonStrings(c["classSkills"].toArray()))
            add(s);
        for (auto skill : speciesCreationSkills(c))
            add(skill);
        if (text("class") == "Rogue")
            for (auto s : jsonStrings(c["expertise"].toArray()))
                add(s, "expertise");
    }
    return effects;
}
void refreshCreation()
{
    if (!creationStatus)
        return;
    QString problem = creationProblem();
    if (!character["creationRequired"].toBool())
        creationStatus->setText("Existing character — review starting proficiencies if needed.");
    else if (!problem.isEmpty())
        creationStatus->setText(
            "Creation incomplete: " + problem +
            " You can autosave a draft; finalize after completing these choices.");
    else
    {
        auto c = character["creationTraining"].toObject();
        creationStatus->setText(
            "Starting proficiencies complete. Class: " +
            jsonStrings(c["classSkills"].toArray()).join(", ") +
            ". Background: " + jsonStrings(c["backgroundSkills"].toArray()).join(", ") + ". " +
            (speciesCreationSkills(c).isEmpty()
                 ? QString()
                 : "Species: " + speciesCreationSkills(c).join(", ") + ". ") +
            (text("class") == "Rogue"
                 ? "Expertise: " + jsonStrings(c["expertise"].toArray()).join(", ") + "."
                 : QString()));
    }
}
bool configureStartingProficiencies()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Required starting proficiencies — 2024");
    dialog.resize(720, 730);
    auto *layout = new QVBoxLayout(&dialog);
    auto *creationSummary =
        new QLabel(text("class") + " · " + text("background") + " · " + text("species") +
                   "\nSelect every required choice before finalizing. These grants update the "
                   "Skills tab automatically.");
    creationSummary->setWordWrap(true);
    layout->addWidget(creationSummary);
    auto c = matchingCreation() ? character["creationTraining"].toObject() : QJsonObject{};
    auto bg = backgroundSkills(text("background"));
    bool custom = bg.isEmpty();
    auto *form = new QFormLayout;
    QComboBox *bgBoxes[2];
    for (int i = 0; i < 2; ++i)
    {
        bgBoxes[i] = new QComboBox;
        bgBoxes[i]->addItem("Choose a background skill");
        bgBoxes[i]->addItems(allowedBackgroundSkills());
        bgBoxes[i]->setCurrentText(c["backgroundSkills"].toArray().at(i).toString());
        if (custom)
            form->addRow("Background skill " + QString::number(i + 1), bgBoxes[i]);
        else
            bgBoxes[i]->deleteLater();
    }
    if (!custom)
        form->addRow("Background grants", new QLabel(bg.join(", ")));
    auto *species = new QComboBox;
    species->setObjectName("firstStartingSpeciesSkill");
    species->addItem("Choose a species skill");
    species->addItems(speciesSkills());
    species->setCurrentText(c["speciesSkill"].toString());
    species->setEnabled(!speciesSkills().isEmpty());
    form->addRow("Species skill", species);
    if (speciesSkills().isEmpty())
        species->setItemText(0, "No skill choice granted by this species");
    auto *species2 = new QComboBox(&dialog);
    species2->setObjectName("secondStartingSpeciesSkill");
    species2->addItem("Choose a second species skill");
    species2->addItems(speciesSkills());
    species2->setCurrentText(c["speciesSkill2"].toString());
    species2->setEnabled(speciesSkillCount() == 2);
    if (speciesSkillCount() == 2)
        form->addRow("Second species skill", species2);
    else
        species2->hide();
    auto fixedSpeciesSkills = jsonStrings(selectedSpecies()["fixedSkills"].toArray());
    if (!fixedSpeciesSkills.isEmpty())
    {
        form->addRow("Species grants", new QLabel(fixedSpeciesSkills.join(", ")));
        for (auto skill : fixedSpeciesSkills)
            if (bg.contains(skill))
            {
                auto *replacementNotice =
                    new QLabel("A fixed species skill overlaps your background. Choose a "
                               "replacement species skill above.");
                replacementNotice->setWordWrap(true);
                form->addRow(replacementNotice);
                break;
            }
    }
    layout->addLayout(form);
    auto *counter = new QLabel;
    layout->addWidget(counter);
    auto *list = new QListWidget;
    list->setObjectName("startingClassSkills");
    layout->addWidget(list, 1);
    for (auto skill : startingSkills(text("class")))
    {
        auto *item = new QListWidgetItem(skill, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(c["classSkills"].toArray().contains(skill) ? Qt::Checked
                                                                       : Qt::Unchecked);
    }
    auto *expertForm = new QFormLayout;
    QComboBox *expert[2];
    for (int i = 0; i < 2; ++i)
    {
        expert[i] = new QComboBox;
        expert[i]->addItem("Choose expertise");
        if (text("class") == "Rogue")
            expertForm->addRow("Starting expertise " + QString::number(i + 1), expert[i]);
        else
            expert[i]->deleteLater();
    }
    layout->addLayout(expertForm);
    auto selected = [=]
    {
        QStringList result;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                result << list->item(i)->text();
        return result;
    };
    auto background = [=]
    {
        return custom ? QStringList{bgBoxes[0]->currentIndex() > 0 ? bgBoxes[0]->currentText()
                                                                   : QString(),
                                    bgBoxes[1]->currentIndex() > 0 ? bgBoxes[1]->currentText()
                                                                   : QString()}
                      : bg;
    };
    auto refresh = [=]
    {
        QSignalBlocker blocker(list);
        auto fixed = background() + fixedSpeciesSkills;
        QString race2 = species2->isEnabled() && species2->currentIndex() > 0
                            ? species2->currentText()
                            : QString();
        QString race = species->currentIndex() > 0 ? species->currentText() : QString();
        for (int i = 0; i < list->count(); ++i)
        {
            auto *item = list->item(i);
            bool unavailable =
                fixed.contains(item->text()) || item->text() == race || item->text() == race2;
            if (unavailable)
                item->setCheckState(Qt::Unchecked);
            item->setFlags(unavailable ? Qt::NoItemFlags
                                       : Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                             Qt::ItemIsUserCheckable);
            item->setToolTip(unavailable ? "Already granted by background or species"
                                         : "Allowed class skill");
        }
        auto skills = selected();
        counter->setText(QString("Class skills: %1 / %2 selected. Background/species grants are "
                                 "separate; duplicate choices are unavailable.")
                             .arg(skills.size())
                             .arg(startingSkillCount(text("class"))));
        auto trained = skills + fixed + extraCreationSkills();
        if (!race.isEmpty())
            trained << race;
        if (!race2.isEmpty())
            trained << race2;
        trained.removeDuplicates();
        trained.removeAll("");
        if (text("class") == "Rogue")
            for (int i = 0; i < 2; ++i)
            {
                QString old = expert[i]->currentText();
                QSignalBlocker b(expert[i]);
                expert[i]->clear();
                expert[i]->addItem("Choose expertise");
                expert[i]->addItems(trained);
                if (expert[i]->findText(old) >= 0)
                    expert[i]->setCurrentText(old);
            }
    };
    connect(list, &QListWidget::itemChanged, &dialog, refresh);
    connect(species, &QComboBox::currentTextChanged, &dialog, refresh);
    connect(species2, &QComboBox::currentTextChanged, &dialog, refresh);
    if (custom)
        for (auto *box : bgBoxes)
            connect(box, &QComboBox::currentTextChanged, &dialog, refresh);
    refresh();
    if (text("class") == "Rogue")
        for (int i = 0; i < 2; ++i)
            expert[i]->setCurrentText(c["expertise"].toArray().at(i).toString());
    auto *notice = new QLabel(
        "Class choices, fixed background skills and species skills are required here. Extra "
        "proficiencies from feats, tools and later class features remain in their own editors. "
        "Rogue expertise can use any proficient skill, including already recorded feat grants.");
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(
        buttons, &QDialogButtonBox::accepted, &dialog,
        [&]
        {
            QJsonArray exp;
            if (text("class") == "Rogue")
                for (auto *box : expert)
                    exp.append(box->currentIndex() > 0 ? box->currentText() : QString());
            QJsonObject candidate{
                {"class", text("class")},
                {"background", text("background")},
                {"species", text("species")},
                {"classSkills", QJsonArray::fromStringList(selected())},
                {"backgroundSkills", QJsonArray::fromStringList(background())},
                {"speciesSkill", species->currentIndex() > 0 ? species->currentText() : QString()},
                {"speciesSkill2", species2->isEnabled() && species2->currentIndex() > 0
                                      ? species2->currentText()
                                      : QString()},
                {"expertise", exp}};
            auto before = character;
            character["creationRequired"] = true;
            character["creationTraining"] = candidate;
            QString problem = creationProblem();
            if (!problem.isEmpty())
            {
                character = before;
                QMessageBox::information(&dialog, "Required choices", problem);
                return;
            }
            auto profile = character["speciesProfile"].toObject();
            if (profile["name"] == character["species"])
            {
                profile["skill"] = candidate["speciesSkill"];
                profile["skill2"] = candidate["speciesSkill2"];
            }
            character["speciesProfile"] = profile;
            dirty = true;
            if (!save())
            {
                character = before;
                dirty = true;
                QMessageBox::warning(&dialog, "Save failed",
                                     "Your previous choices were preserved. Try saving again.");
                return;
            }
            loadWidgets();
            dialog.accept();
        });
    return dialog.exec() == QDialog::Accepted;
}
