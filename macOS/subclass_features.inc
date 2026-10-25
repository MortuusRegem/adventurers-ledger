QJsonObject featureSubclassDefinition(const QString &cls, const QString &name) const
{
    for (auto v : subclassFeatureCatalog())
    {
        auto d = v.toObject();
        if (d["className"] == cls && d["name"] == name)
            return d;
    }
    return {};
}
QJsonArray classFeatureEntries(const QString &cls, int upTo = 20) const
{
    QJsonArray out;
    auto d = featureSubclassDefinition(cls, subclassFor(cls));
    for (auto v : d["features"].toArray())
    {
        auto f = v.toObject();
        if (f["level"].toInt() > upTo)
            continue;
        f["className"] = cls;
        f["subclass"] = d["name"];
        f["source"] = d["source"];
        f["sourceUrl"] = d["sourceUrl"];
        out.append(f);
    }
    return out;
}
int featureLevelValue(QJsonObject value, const QString &cls) const
{
    int n = value["value"].toInt(1), level = classLevel(cls);
    auto by = value["valueByLevel"].toObject();
    for (int i = 1; i <= level; ++i)
        if (by.contains(QString::number(i)))
            n = by[QString::number(i)].toInt();
    QString scale = value["scale"].toString();
    if (scale == "classLevel")
        n *= level;
    else if (scale == "classLevelPlusOne")
        n *= level + 1;
    else if (scale == "classProficiency")
        n *= qMin(6, 2 + (level - 1) / 4);
    else if (scale == "proficiency")
        n *= pb();
    else if (abilities.contains(scale))
        n *= qMax(value["minimum"].toInt(1), ability(abilities.indexOf(scale)));
    return qMax(0, n);
}
QJsonObject featureState(const QJsonObject &f) const
{
    return character["subclassFeatureState"].toObject()[f["id"].toString()].toObject();
}
QStringList featureSelection(const QJsonObject &f) const
{
    return jsonStrings(
        character["subclassFeatureChoices"].toObject()[f["id"].toString()].toArray());
}
int featurePoolCount(const QJsonObject &f) const
{
    auto r = f["resource"].toObject();
    return qBound(0, featureState(f)["poolCount"].toInt(r["initial"].toInt(2)),
                  featureLevelValue(r, f["className"].toString()));
}
QJsonArray newSubclassFeatureEffects() const
{
    QJsonArray out;
    if (!character["manageClassProgression"].toBool())
        return out;
    for (auto roster : classRoster())
    {
        QString cls = roster.toObject()["name"].toString();
        int level = classLevel(cls);
        for (auto v : classFeatureEntries(cls, level))
        {
            auto f = v.toObject();
            QString id = f["id"].toString();
            auto append = [&](QJsonObject e)
            {
                e["source"] = cls + ": " + f["subclass"].toString() + " — " + f["name"].toString();
                e["featureId"] = id;
                if (e["scale"] == "classLevel")
                {
                    e["value"] = e["value"].toInt() * level;
                    e["scale"] = "flat";
                }
                auto by = e["valueByLevel"].toObject();
                for (int n = 1; n <= level; ++n)
                    if (by.contains(QString::number(n)))
                        e["value"] = by[QString::number(n)];
                // Older supported definitions retain ownership of identical permanent targets.
                bool prior = false;
                for (auto old : subclassDefinition(cls)["effects"].toArray())
                {
                    auto o = old.toObject();
                    if (level >= o["level"].toInt(3) && o["target"] == e["target"] &&
                        o["op"].toString("bonus") == e["op"].toString("bonus"))
                        prior = true;
                }
                if (!prior)
                    out.append(e);
            };
            for (auto e : f["effects"].toArray())
                append(e.toObject());
            if (featureState(f)["active"].toBool())
                for (auto e : f["activeEffects"].toArray())
                    append(e.toObject());
            auto c = f["choice"].toObject();
            auto chosen = featureSelection(f);
            for (auto s : chosen)
            {
                if (c["kind"] == "training" && skillNames.contains(s))
                {
                    append(effect("skill:" + s, 0, "flat",
                                  c["expertise"].toBool() ? "expertise" : "proficiency"));
                    if (!c["bonusAbility"].toString().isEmpty())
                    {
                        auto e = effect("skill:" + s, 1, c["bonusAbility"].toString());
                        e["minimumValue"] = c["minimumBonus"].toInt(1);
                        append(e);
                    }
                }
                if (c["kind"] == "save" && abilities.contains(s))
                    append(effect("save:" + s, 0, "flat", "proficiency"));
            }
            if (f["name"] == "Tokens of the Departed" && featurePoolCount(f) > 0)
            {
                append(effect("save:Constitution", 0, "flat", "advantage"));
                append(effect("deathSave", 0, "flat", "advantage"));
            }
            if (f["name"] == "Transmuter's Stone" && featureState(f)["active"].toBool())
            {
                append(effect("save:Constitution", 0, "flat", "proficiency"));
                if (chosen.contains("Speed"))
                    append(effect("speed", 10));
                if (chosen.contains("Mighty Build"))
                    append(effect("save:Strength", 0, "flat", "advantage"));
            }
        }
    }
    return out;
}
int trainingWithoutFeature(const QJsonObject &f, const QString &skill) const
{
    int base = array("skillProfs").at(skillNames.indexOf(skill)).toInt(), upgrades = 0;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (e["featureId"] == f["id"])
            continue;
        if (e["target"] != "skill:*" && e["target"] != "skill:" + skill)
            continue;
        if (e["op"] == "proficiency")
            base = qMax(1, base);
        if (e["op"] == "expertise")
            base = 2;
        if (e["op"] == "upgrade")
            ++upgrades;
    }
    return qMin(2, base + upgrades);
}
int featureChoiceCount(const QJsonObject &f, int level) const
{
    auto c = f["choice"].toObject();
    int count = c["count"].toInt(1);
    for (int i = 1; i <= level; ++i)
        if (c["countByLevel"].toObject().contains(QString::number(i)))
            count = c["countByLevel"].toObject()[QString::number(i)].toInt();
    return count;
}
int saveTrainingWithoutFeature(const QJsonObject &f, const QString &name) const
{
    int index = abilities.indexOf(name);
    int rank = index >= 0 && array("saveProfs").at(index).toBool() ? 1 : 0;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (e["featureId"] != f["id"] &&
            (e["target"] == "save:*" || e["target"] == "save:" + name) && e["op"] == "proficiency")
            rank = 1;
    }
    return rank;
}
bool configureIndividualFeature(const QJsonObject &f, bool additionsOnly = false)
{
    auto c = f["choice"].toObject();
    if (c.isEmpty())
        return true;
    const QString cls = f["className"].toString();
    int level = classLevel(cls);
    if (level < f["level"].toInt())
        return false;
    if (c["replaceOnRest"].toBool() && !featureSelection(f).isEmpty() &&
        featureState(f)["choiceRest"].toInt(-1) == character["subclassRestEpoch"].toInt())
    {
        displayError("This choice can be replaced after your next short or long rest.");
        return false;
    }
    QStringList options = jsonStrings(c["options"].toArray()), required;
    for (int i = options.size() - 1; i >= 0; --i)
    {
        QString s = options[i];
        if (c["optionLevels"].toObject()[s].toInt() > level)
            options.removeAt(i);
        else if (c["untrained"].toBool() && skillNames.contains(s) &&
                 trainingWithoutFeature(f, s) > 0)
            options.removeAt(i);
    }
    for (auto s : jsonStrings(c["requiredSkills"].toArray()))
        if (trainingWithoutFeature(f, s) == 0)
            required << s;
    if (c["tools"].toBool())
        for (auto v : equipmentCatalog)
        {
            auto item = v.toObject();
            if (item["category"] == "Tools" || item["category"] == "Musical instruments")
                options << "Tool: " + item["name"].toString();
        }
    options.removeDuplicates();
    if (c["kind"] == "save")
    {
        QString preferred = c["preferred"].toString();
        if (!preferred.isEmpty() && saveTrainingWithoutFeature(f, preferred) == 0)
            options = {preferred};
        else
            for (int i = options.size() - 1; i >= 0; --i)
            {
                int a = abilities.indexOf(options[i]);
                if (a >= 0 && saveTrainingWithoutFeature(f, options[i]) > 0)
                    options.removeAt(i);
            }
    }
    int count = featureChoiceCount(f, level);
    if (options.size() < count)
    {
        displayError("Not enough eligible options remain for " + f["name"].toString() +
                     ". Review overlapping training and the source's replacement rules.");
        return false;
    }
    QStringList selected;
    QMap<QString, QString> descriptions;
    for (auto option : options)
        descriptions[option] =
            f["description"].toString() + "\n\nChoice: " + option +
            (required.contains(option) ? "\nThis missing fixed skill is required." : "");
    auto initial = featureSelection(f);
    if (initial.isEmpty())
        initial = required;
    QString title = f["subclass"].toString() + ": " + f["name"].toString();
    bool accepted = additionsOnly ? chooseNewAdvanceItems(title, options, count, selected,
                                                          descriptions, featureSelection(f))
                                  : chooseAdvanceItems(title, options, count, selected,
                                                       descriptions, -1, initial);
    if (!accepted)
        return false;
    for (auto r : required)
        if (!selected.contains(r))
        {
            displayError("Include the required skill: " + r);
            return false;
        }
    auto choices = character["subclassFeatureChoices"].toObject();
    choices[f["id"].toString()] = QJsonArray::fromStringList(selected);
    character["subclassFeatureChoices"] = choices;
    if (c["replaceOnRest"].toBool())
    {
        auto all = character["subclassFeatureState"].toObject(), state = featureState(f);
        state["choiceRest"] = character["subclassRestEpoch"].toInt();
        all[f["id"].toString()] = state;
        character["subclassFeatureState"] = all;
    }
    return true;
}
bool featureChoicesComplete(const QJsonObject &f, int level) const
{
    if (f["choice"].toObject().isEmpty())
        return true;
    return featureSelection(f).size() == featureChoiceCount(f, level);
}
QJsonArray featureResources() const
{
    QJsonArray out;
    if (!character["manageClassProgression"].toBool())
        return out;
    for (auto roster : classRoster())
    {
        QString cls = roster.toObject()["name"].toString();
        for (auto v : classFeatureEntries(cls, classLevel(cls)))
        {
            auto f = v.toObject(), r = f["resource"].toObject();
            if (r.isEmpty() || !f["sharedResource"].toString().isEmpty())
                continue;
            r["max"] = featureLevelValue(r, cls);
            for (int n = 1; n <= classLevel(cls); ++n)
                if (f["recoveryByLevel"].toObject().contains(QString::number(n)))
                    r["recovery"] = f["recoveryByLevel"].toObject()[QString::number(n)];
            r["class"] = cls;
            r["name"] = f["subclass"].toString() + ": " + f["name"].toString();
            r["key"] = "Feature:" + f["id"].toString();
            r["featureId"] = f["id"];
            r["feature"] = f;
            bool duplicate = false;
            for (auto old : subclassDefinition(cls)["resources"].toArray())
            {
                auto name = old.toObject()["name"].toString();
                auto normalize = [](QString s)
                { return s.toLower().remove(QRegularExpression("[^a-z]")); };
                duplicate |=
                    normalize(name) == normalize(f["name"].toString()) ||
                    (f["name"] == "Balm of the Summer Court" && name.startsWith("Balm dice")) ||
                    (f["name"] == "Tentacle of the Deep" && name == "Tentacle of the Deeps");
            }
            if (!duplicate)
                out.append(r);
        }
    }
    return out;
}
QJsonObject resourceForFeature(const QJsonObject &f) const
{
    QString cls = f["className"].toString();
    for (auto v : classResources())
    {
        auto r = v.toObject();
        if (!f["sharedResource"].toString().isEmpty() && r["key"] == f["sharedResource"])
            return r;
        if (r["featureId"] == f["id"])
            return r;
        if (r["class"] != cls)
            continue;
        QString name = r["name"].toString(), feature = f["name"].toString();
        auto norm = [](QString s) { return s.toLower().remove(QRegularExpression("[^a-z]")); };
        if (norm(name) == norm(feature) ||
            (feature == "Balm of the Summer Court" && name.startsWith("Balm dice")) ||
            (feature == "Tentacle of the Deep" && name == "Tentacle of the Deeps"))
            return r;
    }
    return {};
}
int trackedResourceSpent(const QJsonObject &r) const
{
    if (r["countBased"].toBool())
        return r["max"].toInt() - featurePoolCount(r["feature"].toObject());
    return character["classResourcesSpent"].toObject()[r["key"].toString()].toInt();
}
void setTrackedResourceSpent(const QJsonObject &r, int used)
{
    used = qBound(0, used, r["max"].toInt());
    if (r["countBased"].toBool())
    {
        auto all = character["subclassFeatureState"].toObject();
        auto state = all[r["featureId"].toString()].toObject();
        state["poolCount"] = r["max"].toInt() - used;
        all[r["featureId"].toString()] = state;
        put("subclassFeatureState", all);
    }
    else
    {
        auto spent = character["classResourcesSpent"].toObject();
        spent[r["key"].toString()] = used;
        put("classResourcesSpent", spent);
    }
}
void restSubclassFeatures(bool longRest)
{
    character["subclassRestEpoch"] = character["subclassRestEpoch"].toInt() + 1;
    auto all = character["subclassFeatureState"].toObject();
    for (auto roster : classRoster())
        for (auto v : classFeatureEntries(roster.toObject()["name"].toString(),
                                          roster.toObject()["level"].toInt()))
        {
            auto f = v.toObject();
            QString id = f["id"].toString();
            auto state = all[id].toObject();
            state["active"] = false;
            if (longRest && f["resource"].toObject()["countBased"].toBool())
                state["poolCount"] = qMax(2, featurePoolCount(f));
            all[id] = state;
        }
    character["subclassFeatureState"] = all;
}
QString featureRollFormula(const QJsonObject &f) const
{
    QString formula = f["roll"].toString();
    int level = classLevel(f["className"].toString());
    int wisdom = ability(4);
    if (formula == "halfSneak")
        return QString::number(((level + 1) / 2 + 1) / 2) + "d6";
    if (formula == "spiritShield")
        return QString::number(level >= 14 ? 4 : level >= 10 ? 3 : 2) + "d6";
    if (formula == "divineFury")
        return "1d6+" + QString::number(level / 2);
    if (formula == "spores")
        return "1d" + QString::number(level >= 14 ? 10 : level >= 10 ? 8 : level >= 6 ? 6 : 4);
    if (formula == "whispersBlades")
        return QString::number(level >= 15 ? 8 : level >= 10 ? 5 : level >= 5 ? 3 : 2) + "d6";
    if (formula == "superiority")
        return QString("1d%1").arg(level >= 18 ? 12 : level >= 10 ? 10 : 8);
    if (formula == "psionic")
        return QString("1d%1").arg(level >= 17 ? 12 : level >= 11 ? 10 : level >= 5 ? 8 : 6);
    if (formula == "jolt")
        return level >= 15 ? "4d6" : "2d6";
    if (formula == "revivingJolt")
        return QString("%1d4").arg(level >= 17 ? 4 : level >= 11 ? 3 : 2);
    if (formula == "WisdomD6")
        return QString::number(qMax(1, wisdom)) + "d6";
    if (formula == "spellfire")
        return level >= 14 ? "1d8" : "1d4";
    if (formula == "planar")
        return level >= 11 ? "2d8" : "1d8";
    if (formula == "swarm")
        return level >= 11 ? "1d8" : "1d6";
    if (formula == "drakeBreath")
        return level >= 15 ? "10d6" : "8d6";
    formula.replace("5+2level", QString::number(5 + 2 * level))
        .replace("5level", QString::number(5 * level));
    formula.replace("level", QString::number(level));
    for (int i = 0; i < abilities.size(); ++i)
        formula.replace(abilities[i], QString::number(ability(i)));
    return formula.replace("+-", "-");
}
bool useSubclassFeature(const QJsonObject &f, bool spend = true)
{
    if (classLevel(f["className"].toString()) < f["level"].toInt())
        return false;
    QString action;
    if (!f["actions"].toArray().isEmpty())
    {
        QStringList selected;
        if (!chooseAdvanceItems(f["name"].toString(), jsonStrings(f["actions"].toArray()), 1,
                                selected, {}))
            return false;
        action = selected.value(0) + ": ";
    }
    auto r = resourceForFeature(f);
    if (spend && !r.isEmpty())
    {
        int used = trackedResourceSpent(r);
        if (used >= r["max"].toInt())
        {
            bool paidWithTrinket = false;
            if (f["subclass"] == "Phantom" &&
                (f["name"] == "Wails from the Grave" || f["name"] == "Ghost Walk"))
                for (auto v : classFeatureEntries("Rogue", classLevel("Rogue")))
                {
                    auto token = v.toObject();
                    if (token["name"] != "Tokens of the Departed" || featurePoolCount(token) <= 0)
                        continue;
                    QStringList selected;
                    if (!chooseAdvanceItems("Soul trinket cost", {"Consume one soul trinket"}, 1,
                                            selected, {}))
                        return false;
                    auto pool = resourceForFeature(token);
                    setTrackedResourceSpent(pool, trackedResourceSpent(pool) + 1);
                    paidWithTrinket = true;
                    log("Soul trinket", "Consumed one for " + f["name"].toString());
                    break;
                }
            if (!paidWithTrinket)
            {
                displayError("No uses remain for " + f["name"].toString() +
                             ". Rest, or apply the source's alternative recharge cost before "
                             "restoring a use.");
                return false;
            }
        }
        else
            setTrackedResourceSpent(r, used + 1);
    }
    QString formula = featureRollFormula(f),
            detail = f["description"].toString().section("\n", 0, 0);
    if (!formula.isEmpty())
    {
        auto roll = rollDice(formula);
        detail = formula + ": " + roll.detail + " = " + QString::number(roll.total) + ". " + detail;
    }
    log(f["subclass"].toString() + ": " + f["name"].toString(),
        action + detail + " Resolve targets and conditional effects at the table.");
    return true;
}
void subclassInitiativeTriggers()
{
    if (!character["manageClassProgression"].toBool() || classLevel("Rogue") < 17 ||
        subclassFor("Rogue") != "Phantom")
        return;
    for (auto v : classFeatureEntries("Rogue", classLevel("Rogue")))
    {
        auto f = v.toObject();
        if (f["name"] == "Tokens of the Departed" && featurePoolCount(f) == 0)
        {
            auto r = resourceForFeature(f);
            setTrackedResourceSpent(r, r["max"].toInt() - 1);
            log("Death's Friend", "Gained one soul trinket when rolling initiative with none.");
        }
    }
}
void updateSubclassFeatureState(const QJsonObject &f, const QJsonObject &state)
{
    auto all = character["subclassFeatureState"].toObject();
    all[f["id"].toString()] = state;
    put("subclassFeatureState", all);
}
void editFeatureCompanion(const QJsonObject &f)
{
    auto model = f["companion"].toObject();
    if (model.isEmpty())
        return;
    QString cls = f["className"].toString();
    int level = classLevel(cls);
    auto state = featureState(f);
    QString form = featureSelection(f).value(0);
    int maximum = model["baseHp"].toInt() + model["hpPerLevel"].toInt() * level;
    if (f["name"] == "Primal Companion" && form == "Sky")
        maximum = 4 + 4 * level;
    int ac = model["baseAc"].toInt();
    if (model["acProficiency"].toBool())
        ac += pb();
    if (abilities.contains(model["acAbility"].toString()))
        ac += ability(abilities.indexOf(model["acAbility"].toString()));
    QDialog dialog(this);
    dialog.setWindowTitle(f["name"].toString() + " — companion tracker");
    QFormLayout layout(&dialog);
    QLabel rules("Maximum HP " + QString::number(maximum) + " · AC " + QString::number(ac) + "\n" +
                 f["description"].toString() +
                 "\nCommand actions, conditions and attacks use the companion's source rules.");
    rules.setWordWrap(true);
    layout.addRow(&rules);
    QSpinBox hp, temp;
    hp.setRange(0, maximum);
    hp.setValue(qMax(0, maximum - state["companionDamage"].toInt()));
    temp.setRange(0, 999);
    temp.setValue(state["companionTemp"].toInt());
    layout.addRow("Current HP", &hp);
    layout.addRow("Temporary HP", &temp);
    QPlainTextEdit notes;
    notes.setPlainText(state["companionNotes"].toString());
    layout.addRow("Conditions / commands", &notes);
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    state["companionDamage"] = maximum - hp.value();
    state["companionTemp"] = temp.value();
    state["companionNotes"] = notes.toPlainText();
    updateSubclassFeatureState(f, state);
}
void refreshSubclassFeatureUi()
{
    if (!subclassFeatureTable)
        return;
    QSignalBlocker blocker(subclassFeatureTable);
    QString selected;
    if (auto *item = subclassFeatureTable->item(subclassFeatureTable->currentRow(), 0))
        selected = item->data(Qt::UserRole).toJsonObject()["id"].toString();
    subclassFeatureTable->setRowCount(0);
    int select = 0;
    for (auto roster : classRoster())
    {
        QString cls = roster.toObject()["name"].toString();
        int level = classLevel(cls);
        for (auto v : classFeatureEntries(cls))
        {
            auto f = v.toObject();
            int needed = f["level"].toInt();
            bool unlocked = level >= needed;
            int row = subclassFeatureTable->rowCount();
            subclassFeatureTable->insertRow(row);
            QString status =
                unlocked ? (featureChoicesComplete(f, level) ? "Available" : "Choose options")
                         : "At class level " + QString::number(needed);
            if (unlocked && f["toggle"].toBool() && featureState(f)["active"].toBool())
                status = "Active";
            auto r = resourceForFeature(f);
            QString uses =
                r.isEmpty() ? "—"
                            : QString::number(qMax(0, r["max"].toInt() - trackedResourceSpent(r))) +
                                  " / " + QString::number(r["max"].toInt());
            QStringList cells{f["name"].toString(), cls + " " + f["subclass"].toString(),
                              QString::number(needed), status, uses};
            for (int col = 0; col < cells.size(); ++col)
            {
                auto *item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                if (col == 0)
                    item->setData(Qt::UserRole, QVariant::fromValue(f));
                if (!unlocked)
                    item->setForeground(QColor("#887b65"));
                subclassFeatureTable->setItem(row, col, item);
            }
            if (f["id"] == selected)
                select = row;
            bool hidden = !subclassFeatureSearch->text().isEmpty() &&
                          !(cells.join(" ") + f["description"].toString())
                               .contains(subclassFeatureSearch->text(), Qt::CaseInsensitive);
            subclassFeatureTable->setRowHidden(
                row, hidden || (!unlocked && !showFutureSubclassFeatures->isChecked()));
        }
    }
    if (subclassFeatureTable->rowCount())
        subclassFeatureTable->selectRow(select);
    updateSubclassFeatureDetail();
}
QJsonObject selectedFeatureEntry() const
{
    if (auto *item = subclassFeatureTable->item(subclassFeatureTable->currentRow(), 0))
        return item->data(Qt::UserRole).toJsonObject();
    return {};
}
void updateSubclassFeatureDetail()
{
    auto f = selectedFeatureEntry();
    bool unlocked = !f.isEmpty() && classLevel(f["className"].toString()) >= f["level"].toInt();
    subclassFeatureDetails->setPlainText(
        f.isEmpty() ? "Select a subclass for a class to see its abilities."
                    : f["name"].toString() + " — " + f["className"].toString() + " level " +
                          QString::number(f["level"].toInt()) + "\n" + f["activation"].toString() +
                          "\n\n" + sourcesLast(f["description"].toString()) + "\n\nSelected: " +
                          featureSelection(f).join(", ") + "\nRoll: " + featureRollFormula(f));
    featureChooseButton->setEnabled(unlocked && !f["choice"].toObject().isEmpty());
    featureUseButton->setEnabled(unlocked);
    featureRollButton->setEnabled(unlocked && !f["roll"].toString().isEmpty());
    featureToggleButton->setEnabled(unlocked && f["toggle"].toBool());
    featureCompanionButton->setEnabled(unlocked && !f["companion"].toObject().isEmpty());
    featureRestoreButton->setEnabled(unlocked && !resourceForFeature(f).isEmpty());
    featureToggleButton->setText(featureState(f)["active"].toBool() ? "End active effect"
                                                                    : "Activate effect");
}
void buildSubclassFeatureUi()
{
    QVBoxLayout *body;
    subclassFeaturePage = page(body);
    auto *intro = new QLabel(
        "Abilities unlock at each class's level, including multiclass characters. Complete "
        "highlighted choices here for existing characters. Use tracks a known limited-use pool and "
        "rolls supported dice; applying damage, targets and other conditional rules remains a "
        "table decision. Roll only previews dice without spending a use.");
    intro->setWordWrap(true);
    body->addWidget(intro);
    subclassFeatureSearch = new QLineEdit;
    subclassFeatureSearch->setPlaceholderText("Find an ability or subclass…");
    body->addWidget(subclassFeatureSearch);
    showFutureSubclassFeatures = new QCheckBox("Show later abilities");
    body->addWidget(showFutureSubclassFeatures);
    subclassFeatureTable =
        table({"Ability", "Class / subclass", "Class level", "Status", "Uses left"});
    subclassFeatureTable->setObjectName("subclassAbilityTable");
    for (int col = 0; col < 5; ++col)
        subclassFeatureTable->horizontalHeader()->setSectionResizeMode(
            col, col < 2 ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
    body->addWidget(subclassFeatureTable, 2);
    subclassFeatureDetails = new QPlainTextEdit;
    subclassFeatureDetails->setReadOnly(true);
    body->addWidget(subclassFeatureDetails, 1);
    auto *actions = new QHBoxLayout;
    featureChooseButton = button("Choose options…",
                                 [this]
                                 {
                                     auto before = character;
                                     auto f = selectedFeatureEntry();
                                     if (!configureIndividualFeature(f))
                                     {
                                         character = before;
                                         return;
                                     }
                                     syncSubclassSpells();
                                     dirty = true;
                                     if (!save())
                                     {
                                         character = before;
                                         displayError("Could not save the ability choice.");
                                     }
                                     loadWidgets();
                                 });
    featureUseButton = button("Use / record trigger",
                              [this]
                              {
                                  useSubclassFeature(selectedFeatureEntry());
                                  refreshSubclassFeatureUi();
                              });
    featureRollButton =
        button("Roll only", [this] { useSubclassFeature(selectedFeatureEntry(), false); });
    featureToggleButton = button("Activate effect",
                                 [this]
                                 {
                                     auto f = selectedFeatureEntry(), state = featureState(f);
                                     if (!state["active"].toBool() && !useSubclassFeature(f))
                                         return;
                                     state["active"] = !state["active"].toBool();
                                     updateSubclassFeatureState(f, state);
                                 });
    featureCompanionButton =
        button("Companion HP…", [this] { editFeatureCompanion(selectedFeatureEntry()); });
    featureRestoreButton = button("Restore / gain one",
                                  [this]
                                  {
                                      auto f = selectedFeatureEntry(), r = resourceForFeature(f);
                                      if (!r.isEmpty())
                                      {
                                          setTrackedResourceSpent(r, trackedResourceSpent(r) - 1);
                                          log(f["name"].toString(),
                                              "Restored one use or gained one item. Apply the "
                                              "source's trigger or alternative cost at the table.");
                                      }
                                  });
    for (auto *b : {featureChooseButton, featureUseButton, featureRollButton, featureToggleButton,
                    featureCompanionButton, featureRestoreButton})
        actions->addWidget(b);
    body->addLayout(actions);
    connect(subclassFeatureTable, &QTableWidget::itemSelectionChanged, this,
            [this] { updateSubclassFeatureDetail(); });
    connect(subclassFeatureSearch, &QLineEdit::textChanged, this,
            [this] { refreshSubclassFeatureUi(); });
    connect(showFutureSubclassFeatures, &QCheckBox::toggled, this,
            [this] { refreshSubclassFeatureUi(); });
    tabs->addTab(scroll(subclassFeaturePage), "Subclass abilities");
}
bool selfTestSubclassFeatureEngine()
{
    auto original = character;
    bool oldDirty = dirty;
    auto finish = [&](QString error)
    {
        if (!error.isEmpty())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly))
                file.write(("Subclass ability engine: " + error).toUtf8());
        }
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return error.isEmpty();
    };
    auto fixture = [&](QString cls, QString sub, int level)
    {
        character = blank();
        character["class"] = cls;
        character["subclass"] = sub;
        character["primaryLevel"] = level;
        character["level"] = level;
        character["manageClassProgression"] = true;
        character["Dexterity"] = 16;
        character["Charisma"] = 16;
        character["Wisdom"] = 14;
    };
    int total = 0;
    QSet<QString> ids;
    if (subclassFeatureCatalog().size() != 130)
        return finish("subclass coverage");
    for (auto v : subclassFeatureCatalog())
    {
        auto d = v.toObject();
        QString cls = d["className"].toString(), sub = d["name"].toString();
        if (!subclasses.value(cls).contains(sub) || d["features"].toArray().isEmpty())
            return finish("missing subclass " + sub);
        fixture(cls, sub, 20);
        if (classFeatureEntries(cls, 20).size() != d["features"].toArray().size())
            return finish("feature lookup " + sub);
        for (auto fv : d["features"].toArray())
        {
            auto f = fv.toObject();
            if (ids.contains(f["id"].toString()) || f["description"].toString().isEmpty() ||
                f["level"].toInt() < 3 || f["level"].toInt() > 20)
                return finish("invalid feature " + f["name"].toString());
            ids.insert(f["id"].toString());
            ++total;
        }
    }
    if (total != 681)
        return finish("feature count");
    fixture("Rogue", "Phantom", 9);
    auto find = [&](QString name)
    {
        for (auto v : classFeatureEntries(text("class"), 20))
            if (v.toObject()["name"] == name)
                return v.toObject();
        return QJsonObject{};
    };
    auto wails = find("Wails from the Grave"), tokens = find("Tokens of the Departed");
    if (featureRollFormula(wails) != "3d6" || featurePoolCount(tokens) != 2 ||
        effectAdvantage(saveTargets(2)) != 1)
        return finish("Phantom initial features");
    auto r = resourceForFeature(wails), pool = resourceForFeature(tokens);
    if (r["max"].toInt() != 3 || pool["max"].toInt() != 2)
        return finish("Phantom budgets");
    if (!useSubclassFeature(wails) || trackedResourceSpent(r) != 1)
        return finish("use tracking");
    useSubclassFeature(wails, false);
    if (trackedResourceSpent(r) != 1)
        return finish("preview consumed a use");
    setTrackedResourceSpent(pool, 2);
    if (featurePoolCount(tokens) != 0 || effectAdvantage(saveTargets(2)) != 0)
        return finish("trinket saving throws");
    restoreClassResources(false);
    if (featurePoolCount(tokens) != 0 || trackedResourceSpent(r) != 1)
        return finish("short rest restored long-rest pool");
    restoreClassResources(true);
    if (featurePoolCount(tokens) != 2 || trackedResourceSpent(r) != 0)
        return finish("long rest recovery");
    character["primaryLevel"] = 17;
    character["level"] = 17;
    pool = resourceForFeature(tokens);
    if (pool["max"].toInt() != 4)
        return finish("trinket level scaling");
    setTrackedResourceSpent(pool, 4);
    subclassInitiativeTriggers();
    subclassInitiativeTriggers();
    if (featurePoolCount(tokens) != 1)
        return finish("Death's Friend initiative");
    syncSubclassSpells();
    if (!alreadyKnowsSpell("Speak with Dead"))
        return finish("Voice of Death grant");
    auto whispers = find("Whispers of the Dead");
    auto choices = character["subclassFeatureChoices"].toObject();
    choices[whispers["id"].toString()] = QJsonArray{"Arcana"};
    character["subclassFeatureChoices"] = choices;
    if (effectTraining({"skill:Arcana"}, 0) != 1 || trainingWithoutFeature(whispers, "Arcana") != 0)
        return finish("Whispers training ownership");
    character = QJsonDocument::fromJson(QJsonDocument(character).toJson()).object();
    if (featureSelection(whispers) != QStringList{"Arcana"})
        return finish("choice persistence");
    loadWidgets();
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == "Subclass abilities")
            tabs->setCurrentIndex(i);
    refreshSubclassFeatureUi();
    if (subclassFeatureTable->rowCount() != classFeatureEntries("Rogue").size())
        return finish("feature UI coverage");
    for (int i = 0; i < subclassFeatureTable->rowCount(); ++i)
        if (subclassFeatureTable->item(i, 0)->data(Qt::UserRole).toJsonObject()["name"] ==
            "Tokens of the Departed")
            subclassFeatureTable->selectRow(i);
    grab().save(QCoreApplication::applicationDirPath() + "/preview-subclass-abilities.png");
    character["subclass"] = "Thief";
    syncSubclassSpells();
    if (alreadyKnowsSpell("Speak with Dead") || effectTraining({"skill:Arcana"}, 0))
        return finish("stale subclass grants");
    fixture("Cleric", "Light Domain", 6);
    auto flare = find("Warding Flare"), radiance = find("Radiance of the Dawn");
    if (resourceForFeature(flare)["recovery"] != "Short or long rest" ||
        resourceForFeature(radiance)["key"] != "Cleric:Channel Divinity")
        return finish("shared resource and recovery upgrade");
    fixture("Sorcerer", "Draconic Sorcery", 3);
    if (classUnarmoredAc() != 16 || effectBonus({"hpMax"}) != 3)
        return finish("Draconic defense");
    fixture("Rogue", "Phantom", 3);
    character["multiclasses"] =
        QJsonArray{QJsonObject{{"name", "Fighter"}, {"level", 10}, {"subclass", "Champion"}}};
    character["level"] = 13;
    if (featureRollFormula(find("Wails from the Grave")) != "1d6")
        return finish("multiclass feature scaling");
    fixture("Rogue", "Phantom", 3);
    whispers = find("Whispers of the Dead");
    bool choiceSeen = false;
    QTimer chooser;
    connect(&chooser, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                {
                    dialog->reject();
                    return;
                }
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setCheckState(list->item(i)->text() == "Arcana" ? Qt::Checked
                                                                                   : Qt::Unchecked);
                choiceSeen = true;
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    chooser.start(30);
    bool accepted = configureIndividualFeature(whispers);
    chooser.stop();
    if (!accepted || !choiceSeen || effectTraining({"skill:Arcana"}, 0) != 1 ||
        featureState(whispers)["choiceRest"].toInt(-1) != character["subclassRestEpoch"].toInt())
        return finish("guided Whispers choice");
    int previousEpoch = character["subclassRestEpoch"].toInt();
    restoreClassResources(false);
    if (character["subclassRestEpoch"].toInt() != previousEpoch + 1 ||
        featureState(whispers)["choiceRest"].toInt(-1) == character["subclassRestEpoch"].toInt())
        return finish("rest did not release Whispers choice");
    return finish({});
}
