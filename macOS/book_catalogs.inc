QJsonObject backgroundReference(const QString &name) const
{
    for (auto v : backgroundCatalog)
    {
        auto b = v.toObject();
        if (b["label"] == name)
            return b;
    }
    return {};
}
QStringList backgroundNames() const
{
    QStringList out;
    for (auto v : backgroundCatalog)
        out << v.toObject()["label"].toString();
    return out;
}
QString backgroundDescription(const QJsonObject &background) const
{
    QString description = background["description"].toString();
    description.replace(" Legacy conversion:", "\nLegacy conversion:");
    QString text = background["name"].toString() + "\n\n" + background["summary"].toString();
    if (!background["publishedFeatureSummary"].toString().isEmpty())
        text +=
            "\n\nBackground feature summary\n" + background["publishedFeatureSummary"].toString();
    text += "\n\n" + description;
    QString status = background["summaryVerification"].toString();
    QString note =
        status == "uploaded-pdf"
            ? "Narrative and feature summary checked directly against your uploaded " +
                  background["summarySourceTitle"].toString() + ", PDF page " +
                  QString::number(background["summarySourcePage"].toInt()) + "."
        : status == "publisher"
            ? "Paraphrase checked against the publisher's public background entry."
        : status == "public-preview"
            ? "Only a public preview was available; the complete book narrative remains unverified."
            : "Paraphrase checked against a public secondary reference; not directly checked "
              "against the book.";
    text += "\nSource: " + note + " Checked " + background["summaryCheckedOn"].toString() + ".\n" +
            background["summaryReferenceUrl"].toString();
    if (background["legacy"].toBool())
        text += "\nAdaptation: Confirm older background features with your DM when using the 2024 "
                "conversion. Narrative benefits and their conditions are resolved at the table.";
    return sourcesLast(text);
}
QStringList backgroundAbilityChoices() const
{
    auto b = backgroundReference(text("background"));
    return b.isEmpty() ? abilities : jsonStrings(b["abilities"].toArray());
}
QStringList allowedBackgroundSkills() const
{
    auto b = backgroundReference(text("background"));
    if (!b["skillChoices"].toArray().isEmpty())
        return jsonStrings(b["skillChoices"].toArray());
    auto fixed = jsonStrings(b["skills"].toArray());
    return fixed.isEmpty() ? skillNames : fixed;
}
QStringList backgroundFeatChoices() const
{
    auto b = backgroundReference(text("background"));
    auto names = jsonStrings(b["featChoices"].toArray());
    if (names.isEmpty())
        for (auto v : featCatalog)
            if (v.toObject()["category"] == "Origin")
                names << v.toObject()["name"].toString();
    return names;
}
void browseBackgrounds()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Background library — books & revised rules");
    dialog.resize(1050, 720);
    QVBoxLayout layout(&dialog);
    QLabel intro(QString::number(backgroundCatalog.size()) +
                 " backgrounds. Search by name or book. Older versions are labeled Legacy; their "
                 "ability increases follow the 2024 conversion.");
    intro.setWordWrap(true);
    layout.addWidget(&intro);
    QLineEdit backgroundSearch;
    backgroundSearch.setPlaceholderText("Search background or book…");
    layout.addWidget(&backgroundSearch);
    QComboBox source;
    source.addItem("All books");
    QStringList sources;
    for (auto v : backgroundCatalog)
        sources << v.toObject()["source"].toString();
    sources.removeDuplicates();
    sources.sort();
    source.addItems(sources);
    layout.addWidget(&source);
    QSplitter split;
    layout.addWidget(&split, 1);
    QListWidget list;
    list.setObjectName("bookBackgroundList");
    split.addWidget(&list);
    QPlainTextEdit details;
    details.setReadOnly(true);
    split.addWidget(&details);
    split.setStretchFactor(1, 2);
    for (auto v : backgroundCatalog)
    {
        auto b = v.toObject();
        auto *item =
            new QListWidgetItem(b["label"].toString() + " · " + b["source"].toString(), &list);
        item->setData(Qt::UserRole, b["label"].toString());
        item->setData(Qt::UserRole + 1, b["source"].toString());
        if (b["label"] == text("background"))
            list.setCurrentItem(item);
    }
    auto filter = [&]
    {
        for (int i = 0; i < list.count(); ++i)
        {
            auto *item = list.item(i);
            item->setHidden(!item->text().contains(backgroundSearch.text(), Qt::CaseInsensitive) ||
                            (source.currentIndex() > 0 &&
                             item->data(Qt::UserRole + 1).toString() != source.currentText()));
        }
    };
    connect(&backgroundSearch, &QLineEdit::textChanged, &dialog, filter);
    connect(&source, &QComboBox::currentTextChanged, &dialog, filter);
    auto show = [&]
    {
        if (auto *item = list.currentItem())
            details.setPlainText(
                backgroundDescription(backgroundReference(item->data(Qt::UserRole).toString())));
    };
    connect(&list, &QListWidget::currentItemChanged, &dialog, show);
    if (!list.currentItem() && list.count())
        list.setCurrentRow(0);
    show();
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Use background");
    buttons.button(QDialogButtonBox::Ok)->setEnabled(!character["abilitiesLocked"].toBool());
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog,
            [&]
            {
                if (list.currentItem() && !list.currentItem()->isHidden())
                    dialog.accept();
            });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
    {
        auto label = list.currentItem()->data(Qt::UserRole).toString();
        qobject_cast<QComboBox *>(fields.value("background"))->setCurrentText(label);
    }
}
void grantBackgroundFeat()
{
    auto names = backgroundFeatChoices();
    for (int i = names.size() - 1; i >= 0; --i)
        if (!canSelectFeat(names[i]))
            names.removeAt(i);
    if (names.isEmpty())
    {
        displayError(
            "The available background feat is already selected. No duplicate can be added.");
        return;
    }
    QStringList chosen;
    QMap<QString, QString> descriptions;
    for (auto name : names)
    {
        auto f = featReference(name);
        descriptions[name] = f["description"].toString() + "\n\n" + f["automation"].toString();
    }
    for (auto v : array("feats"))
        if (!v.toObject()["backgroundGrant"].toString().isEmpty())
        {
            displayError("A background feat has already been granted. Remove the old background "
                         "feat in Feats & bonuses before replacing it.");
            return;
        }
    if (!chooseAdvanceItems("Grant the background feat — choose one", names, 1, chosen,
                            descriptions))
        return;
    auto f = featReference(chosen[0]);
    if (!canSelectFeat(chosen[0]))
    {
        displayError("This feat is already on the character. It counts toward the background "
                     "grant; no duplicate was added.");
        return;
    }
    auto b = backgroundReference(text("background"));
    if (chosen[0] == "Magic Initiate")
    {
        QString description = b["featText"].toString();
        for (auto cls : QStringList{"Cleric", "Druid", "Wizard"})
            if (description.contains("(" + cls + ")") ||
                (b["name"] == "Astral Drifter" && cls == "Cleric"))
                f["forcedSpellList"] = cls;
    }
    if (chosen[0] == "Potent Dragonmark")
    {
        displayError("Select a dragonmark before granting this feat.");
        return;
    }
    if (!configureFeat(f))
        return;
    if (chosen[0] == "Magic Initiate")
        for (auto owned : array("feats"))
            if (owned.toObject()["name"] == "Magic Initiate" &&
                owned.toObject()["spellList"] == f["spellList"])
            {
                displayError("Magic Initiate for this spell list is already recorded. No duplicate "
                             "was added.");
                return;
            }
    f["backgroundGrant"] = text("background");
    auto feats = array("feats");
    feats.append(f);
    put("feats", feats);
    refreshFeats();
    loadWidgets();
}
bool supplementalFeatEligible(const QJsonObject &f) const
{
    if (number("level", 1) < f["minLevel"].toInt(1))
        return false;
    auto required = f["requiresFeat"].toString();
    if (!required.isEmpty() && !hasFeat(required))
        return false;
    if (f["category"] == "Dragonmark" || f["category"] == "Planar Pact")
        for (auto v : array("feats"))
            if (v.toObject()["category"] == f["category"])
                return false;
    QString requirement = f["speciesRequirement"].toString();
    if (!requirement.isEmpty())
    {
        QString species = text("species"),
                lineage = character["speciesProfile"].toObject()["lineage"].toString();
        QString combined = species + " " + lineage;
        bool match = false;
        if (requirement.contains("Small", Qt::CaseInsensitive) &&
            selectedSpecies()["size"] == "Small")
            match = true;
        for (auto candidate : QStringList{"Half-Elf", "Half-Orc", "Human", "Halfling", "Dragonborn",
                                          "Tiefling", "Dwarf", "Gnome", "Elf"})
            if (requirement.contains(candidate, Qt::CaseInsensitive) &&
                species.compare(candidate, Qt::CaseInsensitive) == 0)
                match = true;
        if (!match)
            return false;
        for (auto sub : QStringList{"Drow", "High", "Wood", "Deep"})
            if (requirement.contains(sub, Qt::CaseInsensitive) &&
                !combined.contains(sub, Qt::CaseInsensitive))
                return false;
    }
    return true;
}
bool configureSupplementalSkills(QJsonObject &feat, QJsonArray &effects, const QJsonArray &pending)
{
    auto options = jsonStrings(feat["skillChoices"].toArray());
    QStringList chosen;
    if (!options.isEmpty())
    {
        if (!chooseAdvanceItems(feat["name"].toString() + " — skill proficiencies", options,
                                feat["skillCount"].toInt(1), chosen, {}))
            return false;
        for (auto s : chosen)
            effects.append(effect("skill:" + s, 0, "flat", "proficiency"));
        feat["choices"] = feat["choices"].toString() + "; Skills: " + chosen.join(", ");
    }
    if (feat["expertiseChoice"].toBool())
    {
        options = chosen;
        for (auto s : skillNames)
            if (choiceTraining(s, pending) > 0 && !options.contains(s))
                options << s;
        QStringList expert;
        if (!chooseAdvanceItems(feat["name"].toString() + " — expertise", options, 1, expert, {}))
            return false;
        effects.append(effect("skill:" + expert[0], 0, "flat", "expertise"));
        feat["choices"] = feat["choices"].toString() + "; Expertise: " + expert[0];
    }
    return true;
}
bool selfTestBookCatalogs()
{
    auto snapshot = character;
    auto fail = [&](const QString &reason)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Book catalogs: " + reason).toUtf8());
        character = snapshot;
        loadWidgets();
        return false;
    };
    if (backgroundCatalog.size() != 147 || featCatalog.size() != 264)
        return fail("counts");
    character = blank();
    character["level"] = 20;
    character["feats"] =
        QJsonArray{QJsonObject{{"name", "Magic Initiate"}, {"active", true}},
                   QJsonObject{{"name", "Elemental Adept"}, {"active", true}},
                   QJsonObject{{"name", "Alert"}, {"active", false}},
                   QJsonObject{{"name", "Ability Score Improvement"}, {"active", true}}};
    if (canSelectFeat("Magic Initiate") || canSelectFeat("Elemental Adept") ||
        canSelectFeat("Alert") || !canSelectFeat("Ability Score Improvement") ||
        canSelectFeat("Tough", QJsonArray{QJsonObject{{"name", "Tough"}}}) ||
        advancementFeatEligible(featReference("Alert")))
        return fail("one copy per feat, including inactive and pending feats");
    bool policyChecked = false;
    QTimer::singleShot(0, this,
                       [&]
                       {
                           auto *dialog =
                               qobject_cast<QDialog *>(QApplication::activeModalWidget());
                           if (!dialog)
                               return;
                           auto *list = dialog->findChild<QListWidget *>();
                           int checked = 0;
                           if (list)
                               for (int i = 0; i < list->count(); ++i)
                               {
                                   auto *item = list->item(i);
                                   auto name = item->data(Qt::UserRole).toString();
                                   if (QStringList{"Magic Initiate", "Elemental Adept", "Alert",
                                                   "Ability Score Improvement"}
                                           .contains(name))
                                   {
                                       bool enabled = item->flags().testFlag(Qt::ItemIsEnabled);
                                       bool checkable =
                                           item->flags().testFlag(Qt::ItemIsUserCheckable);
                                       if (enabled == (name == "Ability Score Improvement") &&
                                           checkable == enabled)
                                           ++checked;
                                   }
                               }
                           policyChecked = checked == 4;
                           dialog->reject();
                       });
    selectFeats();
    if (!policyChecked || array("feats").size() != 4)
        return fail("duplicate picker policy");
    character = snapshot;
    QSet<QString> featNames;
    for (auto v : featCatalog)
    {
        auto f = v.toObject();
        if (featNames.contains(f["name"].toString()) || f["description"].toString().isEmpty() ||
            f["automation"].toString().isEmpty() || f["source"].toString().isEmpty())
            return fail("feat data");
        featNames.insert(f["name"].toString());
    }
    QSet<QString> names;
    for (auto v : backgroundCatalog)
    {
        auto b = v.toObject();
        QString label = b["label"].toString();
        if (names.contains(label) || label.isEmpty() || b["source"].toString().isEmpty())
            return fail("background labels");
        if (b["summary"].toString().isEmpty() || b["summaryReferenceUrl"].toString().isEmpty() ||
            !QStringList{"publisher", "public-reference", "public-preview", "uploaded-pdf"}
                 .contains(b["summaryVerification"].toString()))
            return fail(label + " narrative provenance");
        if (b["legacy"].toBool() && b["publishedFeatureSummary"].toString().isEmpty())
            return fail(label + " missing older feature explanation");
        if (b["name"] == "Investigator (SCAG)" &&
            (b["skills"].toArray().contains("Athletics") ||
             !b["skills"].toArray().contains("Investigation")))
            return fail("SCAG Investigator variant proficiency");
        if (b["name"] == "Soldier" && (b["publishedFeatureSummary"].toString().contains(
                                           "Military Rank") != b["legacy"].toBool()))
            return fail("Soldier editions mixed");
        names.insert(label);
        if (b["abilities"].toArray().size() != 3 && !b["legacy"].toBool())
            return fail(label + " abilities");
        for (auto n : jsonStrings(b["featChoices"].toArray()))
            if (featReference(n).isEmpty())
                return fail(label + " feat");
    }
    character = blank();
    character["background"] = "House Cannith Heir";
    if (backgroundSkills(text("background")) != QStringList{"Investigation", "Sleight of Hand"} ||
        !backgroundAbilityChoices().contains("Intelligence") ||
        backgroundAbilityChoices().contains("Wisdom"))
        return fail("Cannith grants");
    character["background"] = "Criminal [Legacy: Player's Handbook]";
    if (!backgroundSkills(text("background")).contains("Deception") ||
        backgroundAbilityChoices().size() != 6)
        return fail("legacy distinction");
    character["level"] = 4;
    character["species"] = "Human";
    if (supplementalFeatEligible(featReference("Dragon Fear")) ||
        supplementalFeatEligible(featReference("Magic Connoisseur")))
        return fail("prerequisites");
    character["species"] = "Dragonborn";
    if (!supplementalFeatEligible(featReference("Dragon Fear")))
        return fail("species eligibility");
    auto initiate = featReference("Magic Initiate");
    initiate["active"] = true;
    character["feats"] = QJsonArray{initiate};
    if (!supplementalFeatEligible(featReference("Magic Connoisseur")))
        return fail("feat chain");
    character = blank();
    character["Dexterity"] = 14;
    loadWidgets();
    auto gunner = featReference("Gunner");
    if (!configureFeat(gunner))
        return fail("Gunner configuration");
    character["feats"] = QJsonArray{gunner};
    if (effectiveScore(1) != 15)
        return fail("Gunner increase");
    auto mobile = featReference("Mobile");
    if (!configureFeat(mobile))
        return fail("Mobile configuration");
    character["feats"] = QJsonArray{mobile};
    if (effectiveSpeed() != 40)
        return fail("Mobile speed");
    mobile["active"] = false;
    character["feats"] = QJsonArray{mobile};
    if (effectiveSpeed() != 30 || number("Dexterity") != 14)
        return fail("removal");
    auto mark = featReference("Mark of Passage"),
         greater = featReference("Greater Mark of Passage");
    mark["active"] = true;
    mark["effects"] = mark["baseEffects"];
    greater["active"] = true;
    greater["effects"] = greater["baseEffects"];
    character["feats"] = QJsonArray{mark, greater};
    if (effectDice({"skill:Athletics"}) != "+1d6")
        return fail("greater mark replacement");
    character["feats"] = QJsonArray{mark};
    if (effectDice({"skill:Athletics"}) != "+1d4")
        return fail("lesser mark restoration");
    character = blank();
    loadWidgets();
    character["abilityBudgetManaged"] = false;
    character["background"] = "Criminal [Legacy: Player's Handbook]";
    for (auto a : abilities)
        character[a] = 10;
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->findChild<QComboBox *>("backgroundIncreaseMode")->setCurrentIndex(1);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    applyBackground();
    if (number("Strength") != 11 || number("Dexterity") != 11 || number("Constitution") != 11 ||
        character["backgroundIncreases"].toObject().size() != 3)
        return fail("legacy three ability allocation");
    character = blank();
    loadWidgets();
    bool previewed = false;
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *list = dialog->findChild<QListWidget *>("bookBackgroundList");
            if (!list || list->count() != 147)
            {
                dialog->reject();
                return;
            }
            auto *query = dialog->findChild<QLineEdit *>();
            query->setText("Cannith");
            for (int i = 0; i < list->count(); ++i)
                if (!list->item(i)->isHidden())
                    list->setCurrentRow(i);
            dialog->grab().save(QCoreApplication::applicationDirPath() +
                                "/preview-background-books.png");
            previewed = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    browseBackgrounds();
    if (!previewed || text("background") != "House Cannith Heir")
        return fail("background browser");
    character["background"] = "Farmer";
    character["feats"] = QJsonArray{};
    loadWidgets();
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            if (auto *list = dialog->findChild<QListWidget *>("levelUpChoices"))
            {
                if (list->count())
                    list->item(0)->setCheckState(Qt::Checked);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            }
            else
                dialog->reject();
        });
    grantBackgroundFeat();
    if (array("feats").size() != 1 || array("feats")[0].toObject()["name"] != "Tough" ||
        array("feats")[0].toObject()["backgroundGrant"] != "Farmer")
        return fail("background feat grant");
    character = snapshot;
    loadWidgets();
    return true;
}
