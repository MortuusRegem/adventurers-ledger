QJsonObject uploadedRuleCatalog() const
{
    static QJsonObject referenceData = []
    {
        QFile file(":/uploaded_rules.json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                              : QJsonObject{};
    }();
    return referenceData;
}
QJsonObject uploadedSource(const QString &id) const
{
    for (auto v : uploadedRuleCatalog()["sources"].toArray())
        if (v.toObject()["id"] == id)
            return v.toObject();
    return {};
}
QString uploadedSpellFormula(const QJsonObject &spell, int slot, QString casting = {},
                             QString dice = {}) const
{
    if (dice.isEmpty())
        dice = spell["rollBase"].toString();
    auto match = QRegularExpression("^(\\d+)d(\\d+)$").match(dice);
    if (!match.hasMatch())
        return {};
    int count = match.captured(1).toInt();
    if (spell["cantripScaling"].toBool())
        count *=
            1 + (number("level", 1) >= 5) + (number("level", 1) >= 11) + (number("level", 1) >= 17);
    else
        count += spell["rollUpcast"].toInt() * qMax(0, slot - spell["level"].toInt());
    QString result = QString::number(count) + "d" + match.captured(2);
    if (spell["addCastingModifier"].toBool())
    {
        int i = abilities.indexOf(casting.isEmpty() ? text("spellAbility") : casting);
        result += sign(ability(i < 0 ? 3 : i));
    }
    return result;
}
void addUploadedMagicItem(const QJsonObject &entry)
{
    QJsonObject item{
        {"name", entry["name"]},
        {"qty", "1"},
        {"weight", ""},
        {"description",
         sourcesLast(entry["description"].toString()) + "\n\n" + entry["automation"].toString()},
        {"notes", "Older PDF printing. Set weight for the actual item; resolve attunement, charges "
                  "and activations manually."},
        {"effects", entry["effects"].toArray()},
        {"equipped", false},
        {"pdfSource", entry["source"]},
        {"pdfPage", entry["sourcePage"]}};
    auto items = array("inventory");
    items.append(item);
    put("inventory", items);
    populateInventory();
    inventoryTable->selectRow(items.size() - 1);
}
void browseUploadedRules(QString initialType = "Documents")
{
    QDialog dialog(this);
    dialog.setWindowTitle("Uploaded rulebooks — original editions");
    dialog.resize(1100, 760);
    QVBoxLayout layout(&dialog);
    QLabel intro(
        "These PDFs contain 2014/2015 rules, adventure references, historical organized-play "
        "guides and playtests. "
        "Edition labels apply to every entry. Older spell options require the Spellbook opt-in; "
        "item activations, charges and conditional effects are tracked manually.");
    intro.setWordWrap(true);
    layout.addWidget(&intro);
    QComboBox type;
    type.addItems({"Documents", "Spells", "Magic items"});
    type.setCurrentText(initialType);
    layout.addWidget(&type);
    QLineEdit referenceSearch;
    referenceSearch.setPlaceholderText("Search title, rules or source…");
    layout.addWidget(&referenceSearch);
    QSplitter split;
    QListWidget list;
    list.setObjectName("uploadedRuleList");
    QPlainTextEdit details;
    details.setReadOnly(true);
    split.addWidget(&list);
    split.addWidget(&details);
    split.setStretchFactor(1, 3);
    layout.addWidget(&split, 1);
    QHBoxLayout actions;
    QPushButton open("Open original PDF"), add("Add selected"), close("Close");
    actions.addWidget(&open);
    actions.addWidget(&add);
    actions.addStretch();
    actions.addWidget(&close);
    layout.addLayout(&actions);
    auto selected = [&]
    {
        return list.currentItem() ? list.currentItem()->data(Qt::UserRole).toJsonObject()
                                  : QJsonObject{};
    };
    auto show = [&]
    {
        auto entry = selected();
        bool document = type.currentText() == "Documents";
        auto source = document ? entry : uploadedSource(entry["source"].toString());
        QString body =
            document ? entry["title"].toString() + "\n" + entry["edition"].toString() + "\n\n" +
                           entry["summary"].toString()
                     : entry["description"].toString() + "\n\n" + entry["automation"].toString();
        details.setPlainText(sourcesLast(
            body + "\n\nSource: " + source["title"].toString() + " — " +
                source["edition"].toString() +
                (document ? "\nPages: " + QString::number(source["pages"].toInt())
                          : "\nPDF page: " + QString::number(entry["sourcePage"].toInt())) +
                "\n" + source["path"].toString(),
            document));
        open.setEnabled(!entry.isEmpty());
        add.setEnabled(
            !entry.isEmpty() && !document &&
            (type.currentText() == "Magic items" || character["allowLegacyPdfRules"].toBool()));
        add.setText(document                         ? "Reference document"
                    : type.currentText() == "Spells" ? "Add to spellbook"
                                                     : "Add reference item to inventory");
    };
    auto populate = [&]
    {
        list.clear();
        QString key = type.currentText() == "Documents" ? "sources"
                      : type.currentText() == "Spells"  ? "spells"
                                                        : "magicItems";
        for (auto value : uploadedRuleCatalog()[key].toArray())
        {
            auto entry = value.toObject();
            QString title = entry["title"].toString(entry["name"].toString());
            if (!(title + " " + entry["description"].toString() + entry["summary"].toString())
                     .contains(referenceSearch.text(), Qt::CaseInsensitive))
                continue;
            auto *item = new QListWidgetItem(title, &list);
            item->setData(Qt::UserRole, entry);
        }
        if (list.count())
            list.setCurrentRow(0);
        else
            show();
    };
    connect(&list, &QListWidget::currentRowChanged, &dialog, [&] { show(); });
    connect(&type, &QComboBox::currentTextChanged, &dialog, [&] { populate(); });
    connect(&referenceSearch, &QLineEdit::textChanged, &dialog, [&] { populate(); });
    connect(&close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(&open, &QPushButton::clicked, &dialog,
            [&]
            {
                auto entry = selected();
                auto source = type.currentText() == "Documents"
                                  ? entry
                                  : uploadedSource(entry["source"].toString());
                QString path = source["path"].toString();
                if (!QFile::exists(path))
                {
                    displayError("The original PDF is not at its recorded location: " + path);
                    return;
                }
                QUrl url = QUrl::fromLocalFile(path);
                if (entry["sourcePage"].toInt())
                    url.setFragment("page=" + QString::number(entry["sourcePage"].toInt()));
                QDesktopServices::openUrl(url);
            });
    connect(&add, &QPushButton::clicked, &dialog,
            [&]
            {
                auto entry = selected();
                if (entry.isEmpty())
                    return;
                if (type.currentText() == "Magic items")
                    addUploadedMagicItem(entry);
                else if (type.currentText() == "Spells" &&
                         character["allowLegacyPdfRules"].toBool())
                {
                    if (alreadyKnowsSpell(entry["name"].toString()))
                    {
                        displayError("This spell is already on the character sheet.");
                        return;
                    }
                    auto spells = array("spells");
                    spells.append(QJsonObject{{"name", entry["name"]},
                                              {"prepared", true},
                                              {"castingAbility", text("spellAbility")}});
                    put("spells", spells);
                    refreshSpells();
                }
                statusBar()->showMessage("Added " + entry["name"].toString(), 5000);
            });
    populate();
    dialog.exec();
}
bool selfTestUploadedRules()
{
    auto original = character;
    bool oldDirty = dirty;
    auto finish = [&](QString error)
    {
        character = original;
        loadWidgets();
        dirty = oldDirty;
        if (!error.isEmpty())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly))
                file.write(("Uploaded PDFs: " + error).toUtf8());
        }
        return error.isEmpty();
    };
    auto refs = uploadedRuleCatalog();
    if (refs["sources"].toArray().size() != 9 || refs["spells"].toArray().size() != 41 ||
        refs["magicItems"].toArray().size() != 48)
        return finish("Catalog counts");
    QSet<QString> names;
    for (auto v : refs["spells"].toArray())
    {
        auto spell = v.toObject();
        auto source = uploadedSource(spell["source"].toString());
        if (names.contains(spell["name"].toString()) || spell["sourcePage"].toInt() < 1 ||
            spell["sourcePage"].toInt() > source["pages"].toInt())
            return finish("Spell source or duplicate");
        names.insert(spell["name"].toString());
    }
    character = blank();
    auto absorb = findSpell("Absorb Elements [EE 2015]");
    if (spellBelongsToClass(absorb, "Wizard"))
        return finish("Legacy spell opt-in");
    character["allowLegacyPdfRules"] = true;
    if (!spellBelongsToClass(absorb, "Wizard") || uploadedSpellFormula(absorb, 3) != "3d6")
        return finish("Spell class/upcast");
    character["level"] = 11;
    if (uploadedSpellFormula(findSpell("Frostbite [EE 2015]"), 0) != "3d6" ||
        uploadedSpellFormula(findSpell("Abi-Dalzim’s Horrid Wilting [EE 2015]"), 8) != "10d8")
        return finish("Original-printing dice");
    for (auto v : refs["magicItems"].toArray())
    {
        auto item = v.toObject();
        if (item["rulesName"] != "Bracers of Defense" && item["rulesName"] != "Ring of Protection")
            continue;
        character = blank();
        item["qty"] = "1";
        item["equipped"] = false;
        character["inventory"] = QJsonArray{item};
        int baseline = effectBonus({"ac"});
        item["equipped"] = true;
        character["inventory"] = QJsonArray{item};
        int expected = item["rulesName"] == "Bracers of Defense" ? 2 : 1;
        if (effectBonus({"ac"}) != baseline + expected)
            return finish("Item activation");
        if (expected == 2)
        {
            character["wearingArmor"] = true;
            if (effectBonus({"ac"}) != baseline)
                return finish("Bracers armor condition");
        }
    }
    character = blank();
    bool uiOkay = true;
    for (QString type : {QString("Documents"), QString("Spells"), QString("Magic items")})
    {
        QTimer::singleShot(
            100, this,
            [&, type]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                {
                    uiOkay = false;
                    return;
                }
                auto *list = dialog->findChild<QListWidget *>("uploadedRuleList");
                int expected = type == "Documents" ? 9 : type == "Spells" ? 41 : 48;
                if (!list || list->count() != expected)
                    uiOkay = false;
                if (type != "Documents")
                {
                    auto *details = dialog->findChild<QPlainTextEdit *>();
                    if (!details || details->toPlainText().contains("Source:") ||
                        details->toPlainText().contains("Sources and version notes") ||
                        details->toPlainText().contains("PDF page:"))
                        uiOkay = false;
                }
                if (type == "Spells")
                    for (auto *b : dialog->findChildren<QPushButton *>())
                        if (b->text() == "Add to spellbook" && b->isEnabled())
                            uiOkay = false;
                dialog->grab().save(QCoreApplication::applicationDirPath() + "/pdf-" + type +
                                    ".png");
                dialog->accept();
            });
        browseUploadedRules(type);
    }
    QTimer::singleShot(100, this,
                       [&]
                       {
                           auto *dialog =
                               qobject_cast<QDialog *>(QApplication::activeModalWidget());
                           if (dialog)
                           {
                               auto *list = dialog->findChild<QListWidget *>();
                               if (!list || list->count() < 100)
                                   uiOkay = false;
                               dialog->grab().save(QCoreApplication::applicationDirPath() +
                                                   "/preview-sources.png");
                               dialog->accept();
                           }
                           else
                               uiOkay = false;
                       });
    browseAllSources();
    return finish(uiOkay ? QString() : QString("Reference library controls"));
}
void browseAllSources()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Sources and version notes — Rules and help");
    dialog.resize(1050, 740);
    QVBoxLayout layout(&dialog);
    QLineEdit query;
    query.setPlaceholderText("Search entry, book, source or version…");
    layout.addWidget(&query);
    QSplitter split;
    QListWidget list;
    QPlainTextEdit details;
    details.setReadOnly(true);
    split.addWidget(&list);
    split.addWidget(&details);
    split.setStretchFactor(1, 3);
    layout.addWidget(&split);
    QList<QPair<QString, QString>> entries;
    std::function<void(QJsonValue, QString)> scan = [&](QJsonValue value, QString context)
    {
        if (value.isArray())
        {
            for (auto child : value.toArray())
                scan(child, context);
            return;
        }
        if (!value.isObject())
            return;
        auto object = value.toObject();
        QString title =
            object["label"].toString(object["name"].toString(object["title"].toString(context)));
        QStringList notes;
        for (auto it = object.begin(); it != object.end(); ++it)
        {
            QString key = it.key();
            if (it.value().isString())
            {
                QString text = it.value().toString();
                if (key.contains("source", Qt::CaseInsensitive) ||
                    key.contains("reference", Qt::CaseInsensitive) ||
                    key.contains("verification", Qt::CaseInsensitive) ||
                    key.contains("checked", Qt::CaseInsensitive) || key == "edition" ||
                    key == "path" || key == "attribution" || key == "adaptation")
                    notes << key + ": " + text;
                else
                {
                    QString formatted = sourcesLast(text, true);
                    QString marker = "Sources and version notes\n";
                    int at = formatted.indexOf(marker);
                    if (at >= 0)
                        notes << formatted.mid(at + marker.size());
                }
            }
            if (it.value().isArray() || it.value().isObject())
                scan(it.value(), title + " / " + key);
        }
        if (object["legacy"].toBool())
            notes << "Older entry: review compatibility with 2024 rules.";
        if (object.contains("sourcePage"))
            notes << "PDF page: " + QString::number(object["sourcePage"].toInt());
        if (!notes.isEmpty())
            entries.append({title, notes.join("\n\n")});
    };
    for (QString filename :
         {"catalog.json", "feats.json", "species.json", "equipment.json", "advancement.json",
          "subclasses.json", "artificer.json", "backgrounds.json", "supplemental_feats.json",
          "subclass_features.json", "uploaded_rules.json", "deities.json"})
    {
        QFile file(":/" + filename);
        if (file.open(QIODevice::ReadOnly))
        {
            auto document = QJsonDocument::fromJson(file.readAll());
            scan(document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()),
                 filename);
        }
    }
    entries.append(QPair<QString, QString>{
        "Carrying capacity",
        "2024 SRD 5.2.1, PDF page 178.\nTiny: Strength × 7.5 lb; Small/Medium: ×15; "
        "Large: ×30; Huge: ×60; Gargantuan: ×120. Drag/lift/push limits are twice "
        "carrying capacity. While dragging/lifting/pushing more than the carrying "
        "limit, Speed can be no more than 5 feet. This is separate from the older "
        "optional encumbrance variant. Coin weight: 50 coins per pound. The app shows "
        "a capacity warning rather than applying an automatic movement penalty."});
    auto populate = [&]
    {
        list.clear();
        for (auto entry : entries)
            if ((entry.first + " " + entry.second).contains(query.text(), Qt::CaseInsensitive))
            {
                auto *item = new QListWidgetItem(entry.first, &list);
                item->setData(Qt::UserRole, entry.second);
            }
        if (list.count())
            list.setCurrentRow(0);
        else
            details.clear();
    };
    connect(&list, &QListWidget::currentRowChanged, &dialog,
            [&]
            {
                details.setPlainText(list.currentItem()
                                         ? list.currentItem()->data(Qt::UserRole).toString()
                                         : QString());
            });
    connect(&query, &QLineEdit::textChanged, &dialog, [&] { populate(); });
    QPushButton close("Close");
    layout.addWidget(&close);
    connect(&close, &QPushButton::clicked, &dialog, &QDialog::accept);
    populate();
    dialog.exec();
}
