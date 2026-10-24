double carryingCapacity() const
{
    static const QMap<QString, double> factors{
        {"Tiny", 7.5}, {"Small / Medium", 15}, {"Large", 30}, {"Huge", 60}, {"Gargantuan", 120}};
    double factor = factors.value(text("carryingSize"), 15);
    QString trait = text("carryingTrait");
    auto profile = character["speciesProfile"].toObject();
    bool natural = profile["enabled"].toBool() && profile["name"] == character["species"] &&
                   QStringList{"Goliath", "Bugbear", "Firbolg"}.contains(text("species"));
    bool doubled =
        trait == "Double capacity" || ((trait.isEmpty() || trait == "Automatic") && natural);
    return effectiveScore(0) * factor * (doubled ? 2 : 1);
}
double carriedWeight(bool *incomplete = nullptr) const
{
    double weight = 0;
    bool invalid = false;
    for (auto value : array("inventory"))
    {
        auto item = value.toObject();
        if (item["name"].toString().trimmed().isEmpty())
            continue;
        bool quantityValid = false, weightValid = false;
        double quantity = item["qty"].toVariant().toDouble(&quantityValid);
        double mass = item["weight"].toVariant().toDouble(&weightValid);
        if (!quantityValid || !std::isfinite(quantity) || quantity < 0)
        {
            invalid = true;
            continue;
        }
        if (quantity == 0)
            continue;
        if (!weightValid || !std::isfinite(mass) || mass < 0 || !std::isfinite(quantity * mass))
        {
            invalid = true;
            continue;
        }
        weight += quantity * mass;
    }
    if (character["includeCoinWeight"].toBool(true))
        for (auto coin : {"cp", "sp", "ep", "gp", "pp"})
            weight += qMax(0, number(coin)) / 50.0;
    if (incomplete)
        *incomplete = invalid;
    return weight;
}
void refreshCarryingCapacity()
{
    if (!weightLabel)
        return;
    bool incomplete = false;
    double weight = carriedWeight(&incomplete), capacity = carryingCapacity();
    bool over = weight > capacity + 0.000001;
    QString balance = over
                          ? QString("OVER CAPACITY by %1 lb").arg(weight - capacity, 0, 'f', 2)
                          : QString("%1 lb remaining").arg(qMax(0.0, capacity - weight), 0, 'f', 2);
    weightLabel->setText(
        QString(
            "Carried: %1%2 lb / Maximum: %3 lb  ·  %4\nStrength %5  ·  Drag / lift / push: %6 lb%7")
            .arg(incomplete ? "at least " : "")
            .arg(weight, 0, 'f', 2)
            .arg(capacity, 0, 'f', 2)
            .arg(balance)
            .arg(effectiveScore(0))
            .arg(capacity * 2, 0, 'f', 2)
            .arg(incomplete ? "\nSome quantities or weights are missing or invalid; enter them for "
                              "a complete total."
                            : ""));
    weightLabel->setStyleSheet(over ? (qApp->palette().color(QPalette::Window).lightness() < 80
                                           ? "color:#ff9b88;font-weight:bold;"
                                           : "color:#9e3027;font-weight:bold;")
                                    : "");
    if (carryingProgress)
    {
        carryingProgress->setValue(qBound(0, int(qMin(100.0, weight / capacity * 100)), 100));
        carryingProgress->setFormat(over ? "Over carrying capacity" : "%p% of carrying capacity");
    }
}
bool selfTestEncumbrance()
{
    auto original = character;
    bool oldDirty = dirty;
    auto finish = [&](QString why)
    {
        character = original;
        loadWidgets();
        dirty = oldDirty;
        if (!why.isEmpty())
        {
            QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (f.open(QIODevice::WriteOnly))
                f.write(("Carrying capacity: " + why).toUtf8());
        }
        return why.isEmpty();
    };
    character = blank();
    character["Strength"] = 10;
    character["inventory"] = QJsonArray{};
    for (auto coin : {"cp", "sp", "ep", "gp", "pp"})
        character[coin] = 0;
    if (carryingCapacity() != 150 || carriedWeight() != 0)
        return finish("Strength baseline");
    character["inventory"] = QJsonArray{
        QJsonObject{{"name", "Rope"}, {"qty", "2"}, {"weight", "5"}, {"equipped", false}},
        QJsonObject{{"name", "Armor"}, {"qty", "1"}, {"weight", "40"}, {"equipped", true}}};
    character["gp"] = 50;
    if (carriedWeight() != 51)
        return finish("Quantity, equipped gear or coin weight");
    character["includeCoinWeight"] = false;
    if (carriedWeight() != 50)
        return finish("Coin weight option");
    character["effectSources"] = QJsonArray{QJsonObject{
        {"name", "Strength bonus"},
        {"effects",
         QJsonArray{QJsonObject{{"target", "score:Strength"}, {"op", "minimum"}, {"value", 19}}}}}};
    if (carryingCapacity() != 285)
        return finish("Effective Strength");
    character["carryingSize"] = "Large";
    if (carryingCapacity() != 570)
        return finish("Large size");
    character["effectSources"] = QJsonArray{};
    character["carryingSize"] = "Small / Medium";
    character["species"] = "Goliath";
    character["speciesProfile"] = QJsonObject{{"name", "Goliath"}, {"enabled", true}};
    if (carryingCapacity() != 300)
        return finish("Powerful Build");
    character["carryingTrait"] = "Normal capacity";
    if (carryingCapacity() != 150)
        return finish("Trait override");
    character["carryingSize"] = "Tiny";
    if (carryingCapacity() != 75)
        return finish("Tiny size");
    character["inventory"] =
        QJsonArray{QJsonObject{{"name", "Load"}, {"qty", "1"}, {"weight", "75"}}};
    loadWidgets();
    if (weightLabel->text().contains("OVER CAPACITY"))
        return finish("Exact limit");
    auto items = array("inventory");
    auto item = items[0].toObject();
    item["weight"] = "75.5";
    items[0] = item;
    put("inventory", items);
    if (!weightLabel->text().contains("OVER CAPACITY by 0.50"))
        return finish("Overweight refresh");
    item["weight"] = "";
    items[0] = item;
    character["inventory"] = items;
    bool missing = false;
    carriedWeight(&missing);
    if (!missing)
        return finish("Missing weights");
    item["qty"] = "0";
    items[0] = item;
    character["inventory"] = items;
    carriedWeight(&missing);
    if (missing)
        return finish("Zero quantity");
    if (!save())
        return finish("Save");
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly))
        return finish("Reload");
    auto saved = QJsonDocument::fromJson(file.readAll()).object();
    if (saved["carryingSize"] != "Tiny" || saved["includeCoinWeight"].toBool(true))
        return finish("Settings persistence");
    character["carryingSize"] = "Small / Medium";
    character["Strength"] = 10;
    item["qty"] = "1";
    item["weight"] = "160";
    items[0] = item;
    character["inventory"] = items;
    loadWidgets();
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == "Inventory")
            tabs->setCurrentIndex(i);
    QApplication::processEvents();
    grab().save(QCoreApplication::applicationDirPath() + "/preview-encumbrance.png");
    return finish({});
}
