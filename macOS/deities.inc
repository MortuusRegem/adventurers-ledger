QJsonArray deityCatalog() const
{
    QFile file(":/deities.json");
    return file.open(QIODevice::ReadOnly)
               ? QJsonDocument::fromJson(file.readAll()).object()["deities"].toArray()
               : QJsonArray{};
}
QStringList deityNames() const
{
    QStringList names{"None", "Undecided", "Multiple deities / pantheon"};
    for (auto value : deityCatalog())
        names << value.toObject()["name"].toString();
    return names;
}
QString deityDescription(QString name) const
{
    if (name.isEmpty() || name == "None")
        return "No chosen deity. Faith is optional for this character; follow the campaign's "
               "requirements for divine characters.";
    if (name == "Undecided")
        return "Your character has not chosen a deity. You can decide during play.";
    if (name == "Multiple deities / pantheon")
        return "Your character honors multiple gods or a whole pantheon. Record their names and "
               "your traditions in Faith notes.";
    for (auto value : deityCatalog())
    {
        auto deity = value.toObject();
        if (deity["name"].toString().compare(name, Qt::CaseInsensitive) == 0)
            return deity["name"].toString() + " — " + deity["portfolio"].toString() + ".\n\n" +
                   deity["description"].toString() +
                   "\n\nDeity alignment: " + deity["alignment"].toString() +
                   ". Your character's alignment is chosen separately.";
    }
    return "Custom deity or faith. Record its beliefs and traditions in Faith notes and agree on "
           "campaign details with your DM.";
}
void refreshDeityDescription()
{
    if (deityDetails)
        deityDetails->setText(deityDescription(text("deity")));
}
bool selfTestDeities()
{
    auto original = character;
    bool oldDirty = dirty;
    auto finish = [&](QString reason)
    {
        character = original;
        loadWidgets();
        dirty = oldDirty;
        if (!reason.isEmpty())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly))
                file.write(("Deities: " + reason).toUtf8());
        }
        return reason.isEmpty();
    };
    auto *selector = qobject_cast<QComboBox *>(fields.value("deity"));
    if (!selector || !selector->isEditable() || deityCatalog().size() != 37)
        return finish("Catalog or editable selector");
    QSet<QString> names;
    for (auto value : deityCatalog())
    {
        auto deity = value.toObject();
        QString name = deity["name"].toString();
        if (name.isEmpty() || names.contains(name) || deity["source"].toString().isEmpty() ||
            deityDescription(name).contains("Source:"))
            return finish("Entry integrity");
        names.insert(name);
    }
    for (QString cls : classes)
    {
        character = blank();
        character["class"] = cls;
        character["abilitiesLocked"] = true;
        loadWidgets();
        int ac = armorClass(), hp = maximumHp(), strength = effectiveScore(0);
        selector->setCurrentText("Mystra");
        if (text("deity") != "Mystra" || !selector->isEnabled() ||
            !deityDetails->text().contains("magic") || armorClass() != ac || maximumHp() != hp ||
            effectiveScore(0) != strength)
            return finish("All-class selection or unintended effects");
    }
    selector->setCurrentText("The Lantern Keeper");
    qobject_cast<QLineEdit *>(fields["faithNotes"])->setText("Protect lost travelers.");
    if (!save())
        return finish("Save");
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly))
        return finish("Reload");
    auto saved = QJsonDocument::fromJson(file.readAll()).object();
    if (saved["deity"] != "The Lantern Keeper" || saved["faithNotes"] != "Protect lost travelers.")
        return finish("Custom faith persistence");
    character = saved;
    loadWidgets();
    if (selector->currentText() != "The Lantern Keeper")
        return finish("Custom selection reloaded");
    character.remove("deity");
    character.remove("faithNotes");
    loadWidgets();
    if (!text("deity").isEmpty() || !deityDetails->text().contains("No chosen deity"))
        return finish("Older character compatibility");
    selector->setCurrentText("Selûne");
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == "Character")
            tabs->setCurrentIndex(i);
    QApplication::processEvents();
    grab().save(QCoreApplication::applicationDirPath() + "/preview-deities.png");
    bool libraryOkay = false;
    auto before = character;
    QTimer::singleShot(
        50, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *search = dialog->findChild<QLineEdit *>("deitySearch");
            auto *list = dialog->findChild<QListWidget *>("deityLibraryList");
            auto *details = dialog->findChild<QPlainTextEdit *>("deityLibraryDescription");
            if (search && list && details)
            {
                search->setText("poetry");
                int visible = 0;
                for (int i = 0; i < list->count(); ++i)
                    if (!list->item(i)->isHidden())
                        ++visible;
                libraryOkay = visible == 1 && details->toPlainText().contains("Milil") &&
                              details->toPlainText().contains("verse") &&
                              !details->toPlainText().contains("Source:");
                dialog->grab().save(QCoreApplication::applicationDirPath() +
                                    "/preview-deity-library.png");
            }
            dialog->reject();
        });
    browseDeities();
    if (!libraryOkay || character != before)
        return finish("Library search, description or cancel");
    QTimer::singleShot(
        50, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->findChild<QLineEdit *>("deitySearch")->setText("Mystra");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    browseDeities();
    if (text("deity") != "Mystra")
        return finish("Library selection");
    return finish({});
}
void browseDeities()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Deity library — gods & faiths");
    dialog.resize(1050, 720);
    QVBoxLayout layout(&dialog);
    QLabel intro("37 Forgotten Realms gods. Available to every class. Search by name, portfolio or "
                 "alignment, or choose a personal faith.");
    intro.setWordWrap(true);
    layout.addWidget(&intro);
    QLineEdit deitySearch;
    deitySearch.setObjectName("deitySearch");
    deitySearch.setPlaceholderText("Search gods, beliefs or alignment…");
    layout.addWidget(&deitySearch);
    QComboBox filter;
    filter.setObjectName("deityFilter");
    filter.addItems({"All gods & faiths", "Forgotten Realms", "Personal choices"});
    layout.addWidget(&filter);
    QSplitter split;
    layout.addWidget(&split, 1);
    QListWidget list;
    list.setObjectName("deityLibraryList");
    split.addWidget(&list);
    QPlainTextEdit details;
    details.setObjectName("deityLibraryDescription");
    details.setReadOnly(true);
    split.addWidget(&details);
    split.setStretchFactor(1, 2);
    QStringList names = deityNames();
    names << "Custom deity / faith";
    for (QString name : names)
    {
        auto *item = new QListWidgetItem(name, &list);
        item->setData(Qt::UserRole, name);
        item->setData(Qt::UserRole + 1, name == "None" || name == "Undecided" ||
                                                name == "Multiple deities / pantheon" ||
                                                name == "Custom deity / faith"
                                            ? "Personal choices"
                                            : "Forgotten Realms");
    }
    QLineEdit customName;
    customName.setObjectName("customDeityName");
    customName.setPlaceholderText("Enter your custom deity or faith name…");
    layout.addWidget(&customName);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Use deity / faith");
    layout.addWidget(&buttons);
    auto show = [&]
    {
        auto *item = list.currentItem();
        bool available = item && !item->isHidden();
        QString name = available ? item->data(Qt::UserRole).toString() : QString();
        bool custom = name == "Custom deity / faith";
        customName.setVisible(custom);
        details.setPlainText(available ? deityDescription(name) : "No matching deities or faiths.");
        buttons.button(QDialogButtonBox::Ok)
            ->setEnabled(available && (!custom || !customName.text().trimmed().isEmpty()));
    };
    auto populate = [&]
    {
        QListWidgetItem *first = nullptr;
        for (int i = 0; i < list.count(); ++i)
        {
            auto *item = list.item(i);
            QString name = item->data(Qt::UserRole).toString();
            bool visible = (name + " " + deityDescription(name))
                               .contains(deitySearch.text(), Qt::CaseInsensitive) &&
                           (filter.currentIndex() == 0 ||
                            item->data(Qt::UserRole + 1).toString() == filter.currentText());
            item->setHidden(!visible);
            if (visible && !first)
                first = item;
        }
        if (!list.currentItem() || list.currentItem()->isHidden())
            list.setCurrentItem(first);
        show();
    };
    connect(&list, &QListWidget::currentRowChanged, &dialog, [&] { show(); });
    connect(&deitySearch, &QLineEdit::textChanged, &dialog, [&] { populate(); });
    connect(&filter, &QComboBox::currentTextChanged, &dialog, [&] { populate(); });
    connect(&customName, &QLineEdit::textChanged, &dialog, [&] { show(); });
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QString selected = text("deity");
    int row = names.indexOf(selected.isEmpty() ? QString("None") : selected);
    if (row < 0)
    {
        row = names.indexOf("Custom deity / faith");
        customName.setText(selected);
    }
    list.setCurrentRow(row);
    populate();
    if (dialog.exec() == QDialog::Accepted && buttons.button(QDialogButtonBox::Ok)->isEnabled())
    {
        QString name = list.currentItem()->data(Qt::UserRole).toString();
        if (name == "Custom deity / faith")
            name = customName.text().trimmed();
        qobject_cast<QComboBox *>(fields.value("deity"))->setCurrentText(name);
    }
}
