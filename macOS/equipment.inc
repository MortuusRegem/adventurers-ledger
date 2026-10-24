bool selfTestEquipment()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](const QString &why)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Equipment: " + why).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    if (equipmentCatalog.size() != 182)
        return fail("catalog size");
    auto index = [&](const QString &name)
    {
        for (int i = 0; i < equipmentCatalog.size(); ++i)
            if (equipmentCatalog[i].toObject()["name"].toString() == name)
                return i;
        return -1;
    };
    character = blank();
    loadWidgets();
    auto before = character;
    if (acquireEquipment({index("Plate Armor")}, 1, true, true) || character != before)
        return fail("unaffordable purchase changed character");
    character["gp"] = 100;
    loadWidgets();
    if (!acquireEquipment({index("Dagger"), index("Rope")}, 2, true, true) ||
        purseCopper() != 9400 || number("gp") != 94 || number("pp") != 0 ||
        array("inventory").size() != 2 || array("weapons").size() != 1)
        return fail("purchase totals");
    if (array("inventory")[0].toObject()["weight"] != "1" ||
        array("inventory")[1].toObject()["qty"] != "2")
        return fail("inventory values");
    if (!acquireEquipment({index("Dagger")}, 1, false, true) || array("weapons").size() != 1 ||
        purseCopper() != 9400)
        return fail("free add or duplicate attacks");
    // Exact change from gold, mixed denominations, and no unexpected conversion to platinum.
    for (auto purse : QList<QJsonObject>{{{"gp", 10}},
                                         {{"gp", 10}, {"sp", 5}, {"cp", 3}, {"ep", 2}, {"pp", 1}},
                                         {{"pp", 1}},
                                         {{"gp", 999999}}})
    {
        character = blank();
        for (auto coin : {"cp", "sp", "ep", "gp", "pp"})
            character[coin] = purse[coin].toInt();
        int cheap = -1;
        for (int i = 0; i < equipmentCatalog.size(); ++i)
            if (equipmentCatalog[i].toObject()["costCp"].toInt() == 1)
            {
                cheap = i;
                break;
            }
        if (cheap < 0)
            return fail("copper-price fixture missing");
        qint64 value = purseCopper();
        int platinum = number("pp");
        if (!acquireEquipment({cheap}, 1, true, false) || purseCopper() != value - 1 ||
            number("pp") > platinum)
            return fail("change from mixed coins");
        if (purse.size() == 1 && purse["gp"].toInt() > 0 &&
            (number("gp") != purse["gp"].toInt() - 1 || number("sp") != 9 || number("cp") != 9))
            return fail("gold purchase did not return silver and copper change");
        auto expected = character;
        if (!save())
            return fail("purchase save");
        QFile file(path());
        if (!file.open(QIODevice::ReadOnly))
            return fail("purchase reload");
        auto saved = QJsonDocument::fromJson(file.readAll()).object();
        for (auto coin : {"cp", "sp", "ep", "gp", "pp"})
            if (saved[coin] != expected[coin])
                return fail("saved purchase coin balance");
    }
    character = blank();
    loadWidgets();
    if (!acquireEquipment({index("Plate Armor"), index("Shield"), index("Leather Armor")}, 1, false,
                          false))
        return fail("armor add");
    setNumber("Strength", 14);
    inventoryTable->item(0, 4)->setCheckState(Qt::Checked);
    inventoryTable->item(1, 4)->setCheckState(Qt::Checked);
    if (armorClass() != 20 || effectiveSpeed() != 20 || effectAdvantage({"skill:Stealth"}) != -1)
        return fail("plate and shield effects");
    inventoryTable->item(2, 4)->setCheckState(Qt::Checked);
    if (armorClass() != 15 || effectiveSpeed() != 30 ||
        array("inventory")[0].toObject()["equipped"].toBool())
        return fail("single armor enforcement");
    inventoryTable->item(1, 4)->setCheckState(Qt::Unchecked);
    if (armorClass() != 13)
        return fail("shield removed");
    bool preview = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *list = dialog->findChild<QTableWidget *>("equipmentCatalog");
            if (!list)
            {
                dialog->reject();
                return;
            }
            list->setCurrentCell(index("Dagger"), 1);
            list->item(index("Dagger"), 0)->setCheckState(Qt::Checked);
            dialog->grab().save(QCoreApplication::applicationDirPath() + "/preview-equipment.png");
            preview = true;
            dialog->reject();
        });
    before = character;
    selectEquipment();
    if (!preview || character != before)
        return fail("cancel changed inventory");
    if (!save())
        return fail("persistence");
    QFile persisted(path());
    if (!persisted.open(QIODevice::ReadOnly) ||
        QJsonDocument::fromJson(persisted.readAll()).object()["inventory"].toArray().size() != 3)
        return fail("saved inventory");
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}
QString equipmentMoney(qint64 copper) const
{
    return QString::number(copper / 100.0, 'f', 2) + " GP";
}
qint64 purseCopper() const
{
    return qint64(number("cp")) + 10LL * number("sp") + 50LL * number("ep") + 100LL * number("gp") +
           1000LL * number("pp");
}
bool acquireEquipment(const QList<int> &indices, int quantity, bool buy, bool addAttacks)
{
    if (indices.isEmpty() || quantity < 1 || quantity > 999)
        return false;
    qint64 cost = 0;
    for (int index : indices)
    {
        if (index < 0 || index >= equipmentCatalog.size())
            return false;
        cost += qint64(equipmentCatalog[index].toObject()["costCp"].toInt()) * quantity;
    }
    if (buy && purseCopper() < cost)
        return false;
    auto inventory = array("inventory"), weapons = array("weapons");
    for (int index : indices)
    {
        auto item = equipmentCatalog[index].toObject();
        item["qty"] = QString::number(quantity);
        item["weight"] = QString::number(item["weight"].toDouble(), 'g', 12);
        item["notes"] = item["description"];
        item["equipped"] = false;
        item["catalogItem"] = true;
        inventory.append(item);
        if (addAttacks && item.contains("weapon"))
        {
            auto weapon = item["weapon"].toObject();
            bool exists = false;
            for (auto v : weapons)
                if (v.toObject()["name"] == weapon["name"])
                    exists = true;
            if (!exists)
            {
                bool ranged = weapon["category"].toString().contains("Ranged");
                bool finesse = weapon["properties"].toString().contains("Finesse");
                weapon["ability"] = ranged || (finesse && ability(1) > ability(0)) ? 1 : 0;
                weapon["ranged"] = ranged;
                weapon["trained"] = false;
                weapon["twoHanded"] = weapon["properties"].toString().contains("Two-Handed");
                weapons.append(weapon);
            }
        }
    }
    // Spend existing small coins first; never exchange the untouched purse into platinum.
    if (buy)
    {
        const QList<QPair<QString, int>> coins{
            {"cp", 1}, {"sp", 10}, {"ep", 50}, {"gp", 100}, {"pp", 1000}};
        qint64 debt = cost;
        for (auto coin : coins)
        {
            qint64 owned = number(coin.first);
            qint64 spent = qMin(owned, (debt + coin.second - 1) / coin.second);
            character[coin.first] = int(owned - spent);
            debt -= spent * coin.second;
            if (debt <= 0)
                break;
        }
        qint64 change = -debt;
        for (auto coin : QList<QPair<QString, int>>{{"gp", 100}, {"sp", 10}, {"cp", 1}, {"ep", 50}})
        {
            int owned = number(coin.first);
            int returned = int(qMin(qint64(999999 - owned), change / coin.second));
            character[coin.first] = owned + returned;
            change -= qint64(returned) * coin.second;
        }
    }
    character["inventory"] = inventory;
    character["weapons"] = weapons;
    loadWidgets();
    put("inventory", inventory);
    return true;
}
void selectEquipment()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Choose equipment — 2024 rules");
    dialog.resize(1000, 760);
    auto *layout = new QVBoxLayout(&dialog);
    auto *filters = new QHBoxLayout;
    auto *searchBox = new QLineEdit;
    searchBox->setPlaceholderText("Search equipment or item details…");
    auto *category = new QComboBox;
    category->addItem("All categories");
    QStringList categories;
    for (auto v : equipmentCatalog)
        if (!categories.contains(v.toObject()["category"].toString()))
            categories << v.toObject()["category"].toString();
    category->addItems(categories);
    filters->addWidget(searchBox, 1);
    filters->addWidget(category);
    layout->addLayout(filters);
    auto *list = table({"Choose", "Item", "Category", "Cost each", "Weight each (lb)"});
    list->setObjectName("equipmentCatalog");
    list->setRowCount(equipmentCatalog.size());
    for (int i = 0; i < equipmentCatalog.size(); ++i)
    {
        auto o = equipmentCatalog[i].toObject();
        auto *check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        check->setCheckState(Qt::Unchecked);
        list->setItem(i, 0, check);
        QStringList cells{o["name"].toString(), o["category"].toString(), o["cost"].toString(),
                          QString::number(o["weight"].toDouble(), 'g', 12)};
        for (int c = 0; c < cells.size(); ++c)
            list->setItem(i, c + 1, new QTableWidgetItem(cells[c]));
    }
    layout->addWidget(list, 2);
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    layout->addWidget(details, 1);
    connect(list, &QTableWidget::currentCellChanged, &dialog,
            [=](int row, int, int, int)
            {
                if (row < 0)
                    return;
                auto o = equipmentCatalog[row].toObject();
                details->setPlainText(o["name"].toString() + "\n" + o["cost"].toString() + " · " +
                                      QString::number(o["weight"].toDouble()) +
                                      " lb per listed unit\n\n" + o["description"].toString());
            });
    auto *actions = new QHBoxLayout;
    auto *quantity = new QSpinBox;
    quantity->setRange(1, 999);
    quantity->setValue(1);
    quantity->setObjectName("equipmentQuantity");
    auto *buy = new QCheckBox("Buy — deduct coins");
    buy->setObjectName("equipmentBuy");
    auto *attacks = new QCheckBox("Add weapons to attack list");
    attacks->setChecked(true);
    actions->addWidget(new QLabel("Quantity of each selected item"));
    actions->addWidget(quantity);
    actions->addWidget(buy);
    actions->addWidget(attacks);
    layout->addLayout(actions);
    auto *total = new QLabel;
    layout->addWidget(total);
    auto selected = [=]
    {
        QList<int> result;
        for (int i = 0; i < list->rowCount(); ++i)
            if (list->item(i, 0)->checkState() == Qt::Checked)
                result << i;
        return result;
    };
    auto recalc = [=]
    {
        qint64 cost = 0;
        double weight = 0;
        auto chosen = selected();
        for (int i : chosen)
        {
            auto o = equipmentCatalog[i].toObject();
            cost += qint64(o["costCp"].toInt()) * quantity->value();
            weight += o["weight"].toDouble() * quantity->value();
        }
        total->setText(QString("%1 selected · Total %2 · %3 lb · Available %4")
                           .arg(chosen.size())
                           .arg(equipmentMoney(cost))
                           .arg(weight, 0, 'f', 2)
                           .arg(equipmentMoney(purseCopper())));
    };
    connect(list, &QTableWidget::itemChanged, &dialog, recalc);
    connect(quantity, &QSpinBox::valueChanged, &dialog, recalc);
    recalc();
    auto filter = [=]
    {
        for (int i = 0; i < equipmentCatalog.size(); ++i)
        {
            auto o = equipmentCatalog[i].toObject();
            bool visible = (category->currentIndex() == 0 ||
                            o["category"].toString() == category->currentText()) &&
                           (o["name"].toString() + " " + o["description"].toString())
                               .contains(searchBox->text(), Qt::CaseInsensitive);
            list->setRowHidden(i, !visible);
        }
    };
    connect(searchBox, &QLineEdit::textChanged, &dialog, filter);
    connect(category, &QComboBox::currentTextChanged, &dialog, filter);
    auto *hint =
        new QLabel("Packs and ammunition use the listed bundle price and weight. Packs stay as one "
                   "inventory entry; their contents appear in details. Added weapons need a "
                   "proficiency review in Weapons. Armor and shields take effect when equipped; "
                   "confirm your armor training. Situational item uses remain manual.");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Add selected");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [=, &dialog]
            {
                if (selected().isEmpty())
                {
                    QMessageBox::information(&dialog, "Choose equipment",
                                             "Check at least one item first.");
                    return;
                }
                if (!acquireEquipment(selected(), quantity->value(), buy->isChecked(),
                                      attacks->isChecked()))
                {
                    QMessageBox::information(&dialog, "Not enough coins",
                                             "You do not have enough coins for this purchase. "
                                             "Uncheck Buy to add starting equipment or loot.");
                    return;
                }
                dialog.accept();
            });
    list->setCurrentCell(0, 1);
    dialog.exec();
}
QJsonObject wornCatalogArmor(bool shield = false) const
{
    for (auto v : array("inventory"))
    {
        auto o = v.toObject();
        if (o["equipped"].toBool() && o["qty"].toString().toDouble() > 0 &&
            !o["armorCategory"].toString().isEmpty() && (o["armorCategory"] == "Shield") == shield)
            return o;
    }
    return {};
}
void showEquipmentDetails()
{
    int row = inventoryTable->currentRow();
    if (row < 0)
        return;
    auto o = array("inventory")[row].toObject();
    QDialog dialog(this);
    dialog.setWindowTitle(o["name"].toString());
    dialog.resize(650, 550);
    auto *layout = new QVBoxLayout(&dialog);
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    details->setPlainText("Cost per unit: " + o["cost"].toString("Not recorded") +
                          "\nWeight per unit: " + o["weight"].toString() +
                          " lb\nQuantity: " + o["qty"].toString() + "\n\n" + o["notes"].toString());
    layout->addWidget(details);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

