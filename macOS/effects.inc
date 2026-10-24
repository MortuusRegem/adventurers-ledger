// All tracked effects are derived from saved sources; base fields are never rewritten.
QJsonArray activeEffects() const
{
    QJsonArray result;
    auto append = [&](const QJsonObject &source)
    {
        for (auto value : source["effects"].toArray())
        {
            auto effect = value.toObject();
            if (effect["condition"] == "unarmoredNoShield" &&
                (!wornCatalogArmor().isEmpty() || character["wearingArmor"].toBool() ||
                 !wornCatalogArmor(true).isEmpty()))
                continue;
            if (effect["condition"].toString() == "armor" &&
                (wornCatalogArmor().isEmpty() && !character["wearingArmor"].toBool()))
                continue;
            effect["source"] = source["name"];
            result.append(effect);
        }
    };
    append(QJsonObject{{"name", "Species: " + text("species")}, {"effects", speciesEffects()}});
    append(QJsonObject{{"name", "Starting proficiencies"}, {"effects", creationEffects()}});
    for (auto key : {"feats", "effectSources"})
        for (auto v : array(key))
        {
            auto source = v.toObject();
            if (source["active"].toBool(true))
                append(source);
        }
    append(QJsonObject{{"name", "Class progression"}, {"effects", classEffects()}});
    append(QJsonObject{{"name", "Subclass features"}, {"effects", subclassEffects()}});
    for (auto v : newSubclassFeatureEffects())
        result.append(v);
    for (auto v : array("inventory"))
    {
        auto source = v.toObject();
        if (source["equipped"].toBool() && source["qty"].toString("1").toDouble() > 0)
            append(source);
    }
    auto armor = wornCatalogArmor();
    if (armor["stealth"] == "Disadvantage")
        result.append(QJsonObject{
            {"target", "skill:Stealth"}, {"op", "disadvantage"}, {"source", "Equipped armor"}});
    QJsonArray consolidated;
    for (auto v : result)
    {
        auto e = v.toObject();
        QString group = e["exclusiveGroup"].toString();
        bool superseded = false;
        if (!group.isEmpty())
            for (auto other : result)
            {
                auto o = other.toObject();
                if (o["exclusiveGroup"] == group && o["priority"].toInt() > e["priority"].toInt())
                    superseded = true;
            }
        if (!superseded)
            consolidated.append(e);
    }
    return consolidated;
}
int effectValue(const QJsonObject &effect) const
{
    int value = effect["value"].toVariant().toInt();
    if (effect["scale"] == "level")
        value *= number("level", 1);
    if (effect["scale"] == "proficiency")
        value *= pb();
    int stat = abilities.indexOf(effect["scale"].toString());
    if (stat >= 0 && !effect["target"].toString().startsWith("score:"))
        value *= ability(stat);
    if (effect.contains("minimumValue"))
        value = qMax(effect["minimumValue"].toInt(), value);
    return value;
}
int effectBonus(const QStringList &targets) const
{
    int result = 0;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (targets.contains(e["target"].toString()) && e["op"].toString("bonus") == "bonus")
            result += effectValue(e);
    }
    return result;
}
QString effectDice(const QStringList &targets) const
{
    QString result;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (targets.contains(e["target"].toString()) && e["op"] == "dice")
        {
            QString dice = e["value"].toString().trimmed();
            if (!dice.isEmpty())
                result += (dice.startsWith('-') || dice.startsWith('+') ? "" : "+") + dice;
        }
    }
    return result;
}
int effectiveScore(int index) const
{
    int value = number(abilities[index], 10), floor = 1;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (e["target"].toString() != "score:" + abilities[index])
            continue;
        if (e["op"] == "minimum")
            floor = qMax(floor, effectValue(e));
        else if (e["op"].toString("bonus") == "bonus")
        {
            int bonus = effectValue(e), cap = e["cap"].toInt(30);
            value = bonus > 0 ? qMax(value, qMin(cap, value + bonus)) : value + bonus;
        }
    }
    return qBound(1, qMax(value, floor), 30);
}
int effectTraining(const QStringList &targets, int base) const
{
    int upgrades = 0;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (!targets.contains(e["target"].toString()))
            continue;
        if (e["op"] == "proficiency")
            base = qMax(base, 1);
        if (e["op"] == "expertise")
            base = 2;
        if (e["op"] == "upgrade")
            ++upgrades;
    }
    return qMin(2, base + upgrades);
}
bool hasFeat(const QString &name) const
{
    for (auto v : array("feats"))
    {
        auto f = v.toObject();
        if (f["name"].toString() == name && f["active"].toBool(true))
            return true;
    }
    return false;
}
bool canSelectFeat(const QString &name, const QJsonArray &pending = {}) const
{
    const QString key = name.trimmed().toCaseFolded();
    if (key == QStringLiteral("ability score improvement"))
        return true;
    for (const auto &feats : {array("feats"), pending})
        for (auto v : feats)
            if (v.toObject()["name"].toString().trimmed().toCaseFolded() == key)
                return false;
    return true;
}
int effectAdvantage(const QStringList &targets) const
{
    bool advantage = false, disadvantage = false;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        if (!targets.contains(e["target"].toString()))
            continue;
        advantage |= e["op"] == "advantage";
        disadvantage |= e["op"] == "disadvantage";
    }
    return int(advantage) - int(disadvantage);
}
QStringList skillTargets(int i) const
{
    return {"check:*", "check:" + abilities[skillAbilities[i]], "skill:*",
            "skill:" + skillNames[i]};
}
QStringList saveTargets(int i) const
{
    return {"save:*", "save:" + abilities[i]};
}
int saveBonus(int i) const
{
    return ability(i) +
           pb() * effectTraining(saveTargets(i), array("saveProfs").at(i).toBool() ? 1 : 0) +
           effectBonus(saveTargets(i));
}
int checkBonus(int i) const
{
    return ability(i) + effectBonus({"check:*", "check:" + abilities[i]});
}
int initiativeBonus() const
{
    return ability(1) + number("initiativeBonus") +
           effectBonus({"initiative", "check:*", "check:Dexterity"});
}
int maximumHp() const
{
    return qMax(1, number("hpMax", 1) +
                       (character["levelUpDraft"].toBool()
                            ? number("level", 1) *
                                  (modifier(number("Constitution")) - number("levelUpConModifier"))
                            : 0) +
                       effectBonus({"hpMax"}) +
                       number("level", 1) * (ability(2) - modifier(number("Constitution", 10))));
}
int armorClass() const
{
    auto armor = wornCatalogArmor(), shield = wornCatalogArmor(true);
    int shieldBonus = shield.isEmpty() ? 0 : 2;
    if (speciesDefinition()["ignoreArmor"].toBool())
        return qMax(speciesNaturalAc(), classUnarmoredAc()) + shieldBonus + effectBonus({"ac"});
    if (!armor.isEmpty())
    {
        int base = armor["ac"].toString().section(' ', 0, 0).toInt();
        QString category = armor["armorCategory"].toString();
        int dex = category == "Heavy Armor" ? 0
                  : category == "Medium Armor"
                      ? qMin(hasFeat("Medium Armor Master") ? 3 : 2, ability(1))
                      : ability(1);
        return qMax(0, base + dex + shieldBonus + effectBonus({"ac"}));
    }
    QString mode = text("armorDexMode");
    int now = ability(1), base = modifier(number("Dexterity", 10));
    int cap = (hasFeat("Medium Armor Master") && character["wearingArmor"].toBool()) ? 3 : 2;
    int dex = mode == "None"         ? 0
              : mode == "Maximum +2" ? qMin(cap, now) - qMin(2, base)
                                     : now - base;
    return qMax(qMax(0, number("ac") + shieldBonus + effectBonus({"ac"}) + dex),
                qMax(speciesNaturalAc(), classUnarmoredAc()) > 0
                    ? qMax(speciesNaturalAc(), classUnarmoredAc()) + shieldBonus +
                          effectBonus({"ac"})
                    : 0);
}
int effectiveSpeed() const
{
    auto armor = wornCatalogArmor();
    int requirement = armor["strength"].toString().section(' ', 1, 1).toInt();
    int penalty = requirement > effectiveScore(0) ? 10 : 0;
    return qMax(0, speciesSpeed() + effectBonus({"speed"}) - penalty);
}
bool rangedWeapon(const QJsonObject &w) const
{
    if (w.contains("ranged"))
        return w["ranged"].toBool();
    for (auto v : catalog["weapons"].toArray())
        if (v.toObject()["name"] == w["name"])
            return v.toObject()["category"].toString().contains("Ranged");
    return false;
}
QStringList weaponTargets(const QJsonObject &w, bool damage) const
{
    QStringList targets{damage ? "weaponDamage" : "weaponAttack",
                        rangedWeapon(w) ? (damage ? "rangedDamage" : "rangedAttack")
                                        : (damage ? "meleeDamage" : "meleeAttack")};
    if (damage)
    {
        QString props = w["properties"].toString();
        if (w["dueling"].toBool() && !rangedWeapon(w) && !w["twoHanded"].toBool() &&
            !props.contains("Two-Handed", Qt::CaseInsensitive))
            targets << "duelingDamage";
        if (w["thrown"].toBool() && props.contains("Thrown", Qt::CaseInsensitive))
            targets << "thrownDamage";
        if (character["attackAction"].toBool(true) && props.contains("Heavy", Qt::CaseInsensitive))
            targets << "heavyAttackDamage";
    }
    return targets;
}
bool weaponTrained(const QJsonObject &w) const
{
    if (classWeaponTrained(w))
        return true;
    if (w["trained"].toBool())
        return true;
    if (hasFeat("Martial Weapon Training"))
        for (auto v : catalog["weapons"].toArray())
        {
            auto reference = v.toObject();
            if (reference["name"] == w["name"] &&
                reference["category"].toString().startsWith("Martial"))
                return true;
        }
    return false;
}
int weaponBonus(const QJsonObject &w, bool damage) const
{
    return weaponAbility(w) + w["magic"].toInt() + (damage ? 0 : (weaponTrained(w) ? pb() : 0)) +
           effectBonus(weaponTargets(w, damage));
}
QString effectSummary(const QJsonObject &source) const
{
    QStringList lines;
    for (auto v : source["effects"].toArray())
    {
        auto e = v.toObject();
        QString target = e["target"].toString(), op = e["op"].toString("bonus");
        for (auto p : effectTargets())
            if (p.first == target)
            {
                target = p.second;
                break;
            }
        QString amount = op == "dice" ? e["value"].toString() : sign(effectValue(e));
        QString line = target + ": " +
                       (op == "bonus" || op == "dice" ? amount
                        : op == "minimum" ? "at least " + QString::number(effectValue(e))
                                          : op);
        if (e["scale"] == "level")
            line += QString(" (%1 × level %2)")
                        .arg(e["value"].toVariant().toInt())
                        .arg(number("level", 1));
        if (e["scale"] == "proficiency")
            line += QString(" (%1 × proficiency %2)").arg(e["value"].toVariant().toInt()).arg(pb());
        if (abilities.contains(e["scale"].toString()))
            line += " (× " + e["scale"].toString() + " modifier)";
        if (e["target"].toString().startsWith("score:") && op == "bonus")
            line += QString("; cap %1").arg(e["cap"].toInt(30));
        if (e["condition"] == "armor")
            line += "; while wearing armor";
        lines << line;
    }
    return lines.isEmpty() ? "No permanent numeric effects; see description." : lines.join("\n");
}
void updateEffectSummary()
{
    if (!effectiveStats)
        return;
    effectiveStats->setText(QString("EFFECTIVE STATS    AC %1    Speed %2 ft    Maximum HP %3")
                                .arg(armorClass())
                                .arg(effectiveSpeed())
                                .arg(maximumHp()));
    QStringList lines;
    for (auto v : activeEffects())
    {
        auto e = v.toObject();
        lines << e["source"].toString() + " — " +
                     effectSummary(QJsonObject{{"effects", QJsonArray{e}}});
    }
    effectiveStats->setToolTip(lines.isEmpty()
                                   ? "No active bonuses. Base AC includes your normal armor and "
                                     "Dexterity; tracked bonuses are added separately."
                                   : lines.join("\n"));
    if (effectsOverview)
    {
        QStringList traits;
        if (hasFeat("Athlete"))
            traits << QString("Climb speed %1 ft").arg(effectiveSpeed());
        if (hasFeat("Blind Fighting") || hasFeat("Skulker"))
            traits << "Blindsight 10 ft";
        if (hasFeat("Boon of Truesight"))
            traits << "Truesight 60 ft";
        if (hasFeat("War Caster"))
            traits << "Concentration advantage (Concentration save button)";
        if (hasFeat("Durable"))
            traits << "Death-save advantage";
        if (!traits.isEmpty())
            lines << "Feat traits: " + traits.join(" · ");
        effectsOverview->setPlainText(lines.isEmpty()
                                          ? "No active effects. Add feats, equip items with "
                                            "effects, or add an ability source below."
                                          : lines.join("\n\n"));
    }
}
const QList<QPair<QString, QString>> &effectTargets() const
{
    static const auto targets = []
    {
        QList<QPair<QString, QString>> out;
        for (auto a : abilities)
            out.append({"score:" + a, a + " score"});
        for (auto p : QList<QPair<QString, QString>>{
                 {"ac", "Armor Class"},
                 {"speed", "Speed (ft)"},
                 {"hpMax", "Maximum HP"},
                 {"initiative", "Initiative"},
                 {"check:*", "All ability checks"},
                 {"save:*", "All saving throws"},
                 {"deathSave", "Death saves only"},
                 {"concentration", "Concentration saves only"},
                 {"skill:*", "All skills"},
                 {"passivePerception", "Passive Perception"},
                 {"weaponAttack", "All weapon attacks"},
                 {"weaponDamage", "All weapon damage"},
                 {"rangedAttack", "Ranged weapon attacks"},
                 {"rangedDamage", "Ranged weapon damage"},
                 {"meleeAttack", "Melee weapon attacks"},
                 {"meleeDamage", "Melee weapon damage"},
                 {"duelingDamage", "Dueling: eligible weapon damage"},
                 {"thrownDamage", "Thrown-attack damage"},
                 {"heavyAttackDamage", "Heavy weapon: own Attack-action damage"},
                 {"spellAttack", "Spell attacks"},
                 {"spellDC", "Spell save DC"},
                 {"spellDamage", "Spell damage"},
                 {"spellHealing", "Spell healing"}})
            out.append(p);
        for (auto a : abilities)
        {
            out.append({"check:" + a, a + " checks"});
            out.append({"save:" + a, a + " saves"});
        }
        for (auto skill : skillNames)
            out.append({"skill:" + skill, skill});
        return out;
    }();
    return targets;
}
bool editEffectSource(QJsonObject &source, const QString &kind)
{
    QDialog dialog(this);
    dialog.setWindowTitle(kind + " — effects");
    dialog.resize(1100, 650);
    QVBoxLayout layout(&dialog);
    QFormLayout identity;
    layout.addLayout(&identity);
    QLineEdit name(source["name"].toString());
    identity.addRow("Name", &name);
    QCheckBox active(kind == "Item" ? "Equipped / active" : "Active");
    active.setChecked(source[kind == "Item" ? "equipped" : "active"].toBool(kind != "Item"));
    identity.addRow(&active);
    QPlainTextEdit description(source["description"].toString(source["notes"].toString()));
    description.setMaximumHeight(95);
    identity.addRow("Description / conditions", &description);
    QLabel note("Add bonuses once, without changing your base stats. An inactive source "
                "contributes nothing. "
                "Use Minimum for score-setting items; Proficiency/Expertise do not stack with "
                "existing training. "
                "Dice effects work on checks, saves, attacks and damage (e.g. 1d4); stat totals "
                "require numbers. "
                "Turn conditional sources on only when their conditions apply.");
    note.setWordWrap(true);
    layout.addWidget(&note);
    QTableWidget table(0, 7);
    table.setHorizontalHeaderLabels(
        {"Stat", "Effect", "Value / dice", "Scale", "Score cap", "Condition", "Note"});
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table.horizontalHeader()->setStretchLastSection(true);
    layout.addWidget(&table, 1);
    auto original = source["effects"].toArray();
    auto add = [&](QJsonObject e)
    {
        int r = table.rowCount();
        table.insertRow(r);
        auto *target = new QComboBox;
        for (auto p : effectTargets())
            target->addItem(p.second, p.first);
        target->setCurrentIndex(qMax(0, target->findData(e["target"].toString())));
        table.setCellWidget(r, 0, target);
        auto *op = new QComboBox;
        for (auto p :
             QList<QPair<QString, QString>>{{"bonus", "Add bonus"},
                                            {"minimum", "Score at least"},
                                            {"proficiency", "Proficient"},
                                            {"expertise", "Expertise"},
                                            {"upgrade", "Proficiency / upgrade to expertise"},
                                            {"advantage", "Advantage"},
                                            {"disadvantage", "Disadvantage"},
                                            {"dice", "Extra dice"}})
            op->addItem(p.second, p.first);
        op->setCurrentIndex(qMax(0, op->findData(e["op"].toString("bonus"))));
        table.setCellWidget(r, 1, op);
        table.setCellWidget(r, 2, new QLineEdit(e["value"].toVariant().toString()));
        auto *scale = new QComboBox;
        scale->addItem("Flat", "flat");
        scale->addItem("Per level", "level");
        scale->addItem("× proficiency", "proficiency");
        for (auto a : abilities)
            scale->addItem("× " + a + " modifier", a);
        scale->setCurrentIndex(qMax(0, scale->findData(e["scale"].toString())));
        table.setCellWidget(r, 3, scale);
        auto *cap = new QSpinBox;
        cap->setRange(1, 30);
        cap->setValue(e["cap"].toInt(30));
        table.setCellWidget(r, 4, cap);
        auto *condition = new QComboBox;
        condition->addItem("Always", "");
        condition->addItem("Wearing armor", "armor");
        condition->setCurrentIndex(qMax(0, condition->findData(e["condition"].toString())));
        table.setCellWidget(r, 5, condition);
        table.setCellWidget(r, 6, new QLineEdit(e["note"].toString()));
    };
    for (auto v : original)
        add(v.toObject());
    QHBoxLayout controls;
    layout.addLayout(&controls);
    QPushButton plus("Add modifier"), minus("Remove selected modifier");
    controls.addWidget(&plus);
    controls.addWidget(&minus);
    connect(&plus, &QPushButton::clicked, &dialog,
            [&] { add(QJsonObject{{"target", "ac"}, {"op", "bonus"}, {"value", 1}}); });
    connect(&minus, &QPushButton::clicked, &dialog,
            [&]
            {
                if (table.currentRow() >= 0)
                    table.removeRow(table.currentRow());
            });
    QDialogButtonBox actions(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout.addWidget(&actions);
    connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(
        &actions, &QDialogButtonBox::accepted, &dialog,
        [&]
        {
            if (name.text().trimmed().isEmpty())
            {
                QMessageBox::warning(&dialog, "Effects", "Enter a source name.");
                return;
            }
            QJsonArray effects;
            for (int r = 0; r < table.rowCount(); ++r)
            {
                QString target =
                    qobject_cast<QComboBox *>(table.cellWidget(r, 0))->currentData().toString();
                QString op =
                    qobject_cast<QComboBox *>(table.cellWidget(r, 1))->currentData().toString();
                QString value = qobject_cast<QLineEdit *>(table.cellWidget(r, 2))->text().trimmed();
                QString scale =
                    qobject_cast<QComboBox *>(table.cellWidget(r, 3))->currentData().toString();
                bool training = op == "proficiency" || op == "expertise" || op == "upgrade",
                     adv = op == "advantage" || op == "disadvantage";
                bool rollTarget = target.startsWith("check:") || target.startsWith("save:") ||
                                  target.startsWith("skill:") || target == "initiative" ||
                                  target == "deathSave" || target == "concentration" ||
                                  target.endsWith("Attack") || target.endsWith("Damage") ||
                                  target == "spellHealing";
                bool d20Target =
                    rollTarget && !target.endsWith("Damage") && target != "spellHealing";
                if ((target.startsWith("score:") && abilities.contains(scale)) ||
                    (op == "minimum" && !target.startsWith("score:")) ||
                    (training && !target.startsWith("skill:") && !target.startsWith("save:")) ||
                    (adv && !d20Target) || (op == "dice" && !rollTarget))
                {
                    QMessageBox::warning(&dialog, "Effects",
                                         "This effect type does not apply to the selected stat. "
                                         "Use a numeric bonus for stat totals.");
                    return;
                }
                if (op == "dice")
                {
                    try
                    {
                        if (value.isEmpty() || scale != "flat")
                            throw std::runtime_error(
                                "Extra dice must have a flat scale and a dice expression.");
                        rollDice(value);
                    }
                    catch (const std::exception &e)
                    {
                        QMessageBox::warning(&dialog, "Effects", QString::fromUtf8(e.what()));
                        return;
                    }
                }
                else if (!training && !adv)
                {
                    bool ok;
                    int n = value.toInt(&ok);
                    if (!ok || std::abs(n) > 1000)
                    {
                        QMessageBox::warning(&dialog, "Effects",
                                             "Enter a numeric modifier between -1000 and 1000.");
                        return;
                    }
                }
                effects.append(QJsonObject{
                    {"target", target},
                    {"op", op},
                    {"value", op == "dice" ? QJsonValue(value) : QJsonValue(value.toInt())},
                    {"scale", scale},
                    {"cap", qobject_cast<QSpinBox *>(table.cellWidget(r, 4))->value()},
                    {"condition",
                     qobject_cast<QComboBox *>(table.cellWidget(r, 5))->currentData().toString()},
                    {"note", qobject_cast<QLineEdit *>(table.cellWidget(r, 6))->text()}});
            }
            source["name"] = name.text().trimmed();
            source["description"] = description.toPlainText();
            source["effects"] = effects;
            source[kind == "Item" ? "equipped" : "active"] = active.isChecked();
            dialog.accept();
        });
    return dialog.exec() == QDialog::Accepted;
}
QJsonObject featReference(const QString &name) const
{
    for (auto v : featCatalog)
        if (v.toObject()["name"].toString() == name)
            return v.toObject();
    return {};
}
QJsonObject effect(const QString &target, int value, const QString &scale = "flat",
                   const QString &op = "bonus", int cap = 30) const
{
    return {{"target", target}, {"value", value}, {"scale", scale}, {"op", op}, {"cap", cap}};
}
int choiceTraining(const QString &skill, const QJsonArray &pending) const
{
    int index = skillNames.indexOf(skill);
    int training =
        effectTraining({"skill:*", "skill:" + skill}, array("skillProfs").at(index).toInt());
    QSet<QString> saved;
    for (auto v : array("feats"))
        saved.insert(v.toObject()["id"].toString());
    for (auto v : pending)
    {
        auto f = v.toObject();
        if (!f["active"].toBool(true) || saved.contains(f["id"].toString()))
            continue;
        for (auto ev : f["effects"].toArray())
        {
            auto e = ev.toObject();
            if (e["target"].toString() != "skill:*" && e["target"].toString() != "skill:" + skill)
                continue;
            if (e["op"] == "proficiency")
                training = qMax(training, 1);
            if (e["op"] == "expertise")
                training = 2;
        }
    }
    return training;
}
bool configureFeat(QJsonObject &feat, const QJsonArray &pending = {})
{
    QString name = feat["name"].toString();
    if (!canSelectFeat(name, pending))
    {
        displayError("This feat is already selected. Only Ability Score Improvement can be "
                     "selected more than once.");
        return false;
    }
    QJsonArray effects = feat["baseEffects"].toArray();
    feat["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    feat["active"] = true;
    if (name == "Alert")
        effects.append(effect("initiative", 1, "proficiency"));
    if (name == "Tough")
        effects.append(effect("hpMax", 2, "level"));
    if (name == "Archery")
        effects.append(effect("rangedAttack", 2));
    if (name == "Defense")
    {
        auto e = effect("ac", 1);
        e["condition"] = "armor";
        effects.append(e);
    }
    QStringList allowed;
    int increases = 0, cap = 20;
    if (name == "Ability Score Improvement")
    {
        allowed = abilities;
        increases = 2;
    }
    if (name == "Grappler")
    {
        allowed = {"Strength", "Dexterity"};
        increases = 1;
    }
    if (name.startsWith("Boon of "))
    {
        allowed = name == "Boon of Irresistible Offense" ? QStringList{"Strength", "Dexterity"}
                  : name == "Boon of Spell Recall"
                      ? QStringList{"Intelligence", "Wisdom", "Charisma"}
                      : abilities;
        increases = 1;
        cap = 30;
    }
    if (feat.contains("abilityChoices"))
    {
        allowed.clear();
        for (auto v : feat["abilityChoices"].toArray())
            allowed << v.toString();
        increases = feat["increases"].toInt(1);
        cap = feat["scoreCap"].toInt(20);
    }
    if (name == "Resilient")
    {
        QStringList eligible;
        for (auto a : allowed)
        {
            int i = abilities.indexOf(a);
            if (effectTraining(saveTargets(i), array("saveProfs").at(i).toBool() ? 1 : 0) == 0)
                eligible << a;
        }
        allowed = eligible;
        if (allowed.isEmpty())
        {
            displayError("Resilient requires an ability without saving-throw proficiency.");
            return false;
        }
    }
    QString selectedAbility;
    QJsonObject allocation;
    if (increases == 1 && allowed.size() == 1)
    {
        if (permanentAbilityScore(abilities.indexOf(allowed[0]), pending) >= cap)
            feat["abilityCapped"] = true;
        allocation[allowed[0]] = 1;
    }
    else if (increases &&
             !allocateFeatAbilities(name, allowed, increases, cap, pending, allocation))
        return false;
    if (increases)
    {
        QStringList choices;
        int spent = 0;
        for (auto a : abilities)
            if (allocation[a].toInt())
            {
                if (selectedAbility.isEmpty())
                    selectedAbility = a;
                int amount = allocation[a].toInt();
                spent += amount;
                effects.append(effect("score:" + a, amount, "flat", "bonus", cap));
                choices << a + " +" + QString::number(amount);
            }
        feat["choices"] = choices.join(", ");
        feat["abilityBudget"] = increases;
        feat["abilitySpent"] = spent;
        feat["abilityAllocated"] = allocation;
    }
    auto pick = [&](const QString &label, const QStringList &options, QString &value)
    {
        if (options.isEmpty())
        {
            displayError("No eligible choice remains for " + label + ".");
            return false;
        }
        QMap<QString, QString> descriptions;
        for (auto option : options)
            descriptions[option] =
                feat["description"].toString() + "\n\n" + feat["automation"].toString();
        QStringList selected;
        if (!chooseAdvanceItems(name + " — " + label, options, 1, selected, descriptions))
            return false;
        value = selected.value(0);
        return true;
    };
    auto record = [&](const QString &value)
    { feat["choices"] = feat["choices"].toString() + "; " + value; };
    if (name == "Resilient")
        effects.append(effect("save:" + selectedAbility, 0, "flat", "proficiency"));
    if (name == "Keen Mind" || name == "Observant")
    {
        QString skill;
        QStringList options =
            name == "Keen Mind"
                ? QStringList{"Arcana", "History", "Investigation", "Nature", "Religion"}
                : QStringList{"Insight", "Investigation", "Perception"};
        if (!pick("Skill (proficiency, or expertise if proficient)", options, skill))
            return false;
        effects.append(effect("skill:" + skill, 0, "flat",
                              choiceTraining(skill, pending) > 0 ? "expertise" : "proficiency"));
        record("Skill: " + skill);
    }
    if (name == "Skill Expert" || name == "Boon of Skill")
    {
        QString proficiencySkill, expertSkill;
        if (name == "Skill Expert")
        {
            QStringList options;
            for (int i = 0; i < skillNames.size(); ++i)
                if (choiceTraining(skillNames[i], pending) == 0)
                    options << skillNames[i];
            if (options.isEmpty())
                options = skillNames;
            if (!pick("New skill proficiency", options, proficiencySkill))
                return false;
            effects.append(effect("skill:" + proficiencySkill, 0, "flat", "proficiency"));
            record("Proficient: " + proficiencySkill);
        }
        QStringList options;
        for (int i = 0; i < skillNames.size(); ++i)
        {
            int training = choiceTraining(skillNames[i], pending);
            if (training < 2 &&
                (training > 0 || skillNames[i] == proficiencySkill || name == "Boon of Skill"))
                options << skillNames[i];
        }
        if (!pick("Skill expertise", options, expertSkill))
            return false;
        effects.append(effect("skill:" + expertSkill, 0, "flat", "expertise"));
        record("Expertise: " + expertSkill);
    }
    if (name == "Elemental Adept")
    {
        QString element;
        if (!pick("Damage type", {"Acid", "Cold", "Fire", "Lightning", "Thunder"}, element))
            return false;
        feat["damageType"] = element;
        record(element);
    }
    if (name == "Boon of Energy Resistance")
    {
        QString first, second;
        QStringList types{"Acid",   "Cold",    "Fire",    "Lightning", "Necrotic",
                          "Poison", "Psychic", "Radiant", "Thunder"};
        if (!pick("First resistance", types, first))
            return false;
        types.removeAll(first);
        if (!pick("Second resistance", types, second))
            return false;
        feat["resistances"] = QJsonArray{first, second};
        record("Resistance: " + first + ", " + second);
    }
    if (name == "Weapon Master")
    {
        QStringList weapons;
        for (auto v : catalog["weapons"].toArray())
            weapons << v.toObject()["name"].toString();
        QString weapon;
        if (!pick("Mastery weapon (must be proficient)", weapons, weapon))
            return false;
        record("Mastery: " + weapon);
    }
    if (name == "Skilled")
    {
        QStringList options = skillNames, selected;
        QMap<QString, QString> descriptions;
        for (auto v : equipmentCatalog)
        {
            auto item = v.toObject();
            QString category = item["category"].toString();
            if (category == "Tools" || category == "Musical instruments")
            {
                QString tool = "Tool: " + item["name"].toString();
                options << tool;
                descriptions[tool] = item["description"].toString();
            }
        }
        if (!chooseAdvanceItems("Skilled — choose three skill or tool proficiencies", options, 3,
                                selected, descriptions))
            return false;
        for (auto choice : selected)
            if (skillNames.contains(choice))
                effects.append(effect("skill:" + choice, 0, "flat", "proficiency"));
        feat["choices"] = selected.join(", ");
    }
    if (name == "Magic Initiate")
    {
        bool ok = false;
        QString list = feat["forcedSpellList"].toString();
        if (list.isEmpty())
        {
            list = QInputDialog::getItem(this, name, "Spell list", {"Cleric", "Druid", "Wizard"}, 0,
                                         false, &ok);
            if (!ok)
                return false;
        }
        QString stat = QInputDialog::getItem(this, name, "Casting ability",
                                             {"Intelligence", "Wisdom", "Charisma"}, 0, false, &ok);
        if (!ok)
            return false;
        feat["choices"] = list + " / " + stat;
        feat["spellList"] = list;
    }
    if (!configureSupplementalSkills(feat, effects, pending))
        return false;
    feat["effects"] = effects;
    return true;
}
void refreshFeats()
{
    if (!featTable)
        return;
    QSignalBlocker blocker(featTable);
    int selected = featTable->currentRow();
    featTable->setRowCount(0);
    auto feats = array("feats");
    for (int i = 0; i < feats.size(); ++i)
    {
        auto f = feats[i].toObject();
        featTable->insertRow(i);
        auto *active = new QTableWidgetItem;
        active->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        active->setCheckState(f["active"].toBool(true) ? Qt::Checked : Qt::Unchecked);
        featTable->setItem(i, 0, active);
        featTable->setItem(i, 1, new QTableWidgetItem(f["name"].toString()));
        featTable->setItem(i, 2, new QTableWidgetItem(f["category"].toString("Custom")));
        featTable->setItem(i, 3, new QTableWidgetItem(effectSummary(f)));
    }
    if (selected >= 0 && selected < feats.size())
        featTable->selectRow(selected);
    refreshSources();
}
void refreshSources()
{
    if (!sourceTable)
        return;
    QSignalBlocker blocker(sourceTable);
    sourceTable->setRowCount(0);
    for (auto v : array("effectSources"))
    {
        auto s = v.toObject();
        int r = sourceTable->rowCount();
        sourceTable->insertRow(r);
        auto *on = new QTableWidgetItem;
        on->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        on->setCheckState(s["active"].toBool(true) ? Qt::Checked : Qt::Unchecked);
        sourceTable->setItem(r, 0, on);
        sourceTable->setItem(r, 1, new QTableWidgetItem(s["name"].toString()));
        sourceTable->setItem(r, 2, new QTableWidgetItem(effectSummary(s)));
    }
}
void selectFeats()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Choose feats — select multiple");
    dialog.resize(1000, 650);
    QVBoxLayout layout(&dialog);
    QLabel intro(QString::number(featCatalog.size()) +
                 " feats from the core books and supplements. Search by feat or book, and check "
                 "multiple entries. Legacy options are labeled; read prerequisites and "
                 "manual-tracking notes before adding.");
    intro.setWordWrap(true);
    layout.addWidget(&intro);
    QLineEdit featSearch;
    featSearch.setPlaceholderText("Find a feat…");
    layout.addWidget(&featSearch);
    QComboBox bookFilter;
    bookFilter.addItem("All books");
    QStringList books;
    for (auto v : featCatalog)
        books << v.toObject()["source"].toString();
    books.removeDuplicates();
    books.sort();
    bookFilter.addItems(books);
    layout.addWidget(&bookFilter);
    QSplitter split;
    layout.addWidget(&split, 1);
    QListWidget list;
    split.addWidget(&list);
    QPlainTextEdit details;
    details.setReadOnly(true);
    split.addWidget(&details);
    split.setStretchFactor(1, 2);
    for (auto v : featCatalog)
    {
        auto f = v.toObject();
        auto *item = new QListWidgetItem(f["name"].toString() + " · " + f["category"].toString() +
                                             " · " + f["source"].toString(),
                                         &list);
        item->setData(Qt::UserRole, f["name"].toString());
        item->setData(Qt::UserRole + 1, f["source"].toString());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        if (!canSelectFeat(f["name"].toString()))
        {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsUserCheckable);
            item->setToolTip("Already selected on this character. Only Ability Score Improvement "
                             "can be selected again.");
        }
    }
    auto filterBooks = [&]
    {
        for (int i = 0; i < list.count(); ++i)
        {
            auto *item = list.item(i);
            item->setHidden(!item->text().contains(featSearch.text(), Qt::CaseInsensitive) ||
                            (bookFilter.currentIndex() > 0 &&
                             item->data(Qt::UserRole + 1).toString() != bookFilter.currentText()));
        }
    };
    connect(&featSearch, &QLineEdit::textChanged, &dialog, filterBooks);
    connect(&bookFilter, &QComboBox::currentTextChanged, &dialog, filterBooks);
    connect(&list, &QListWidget::currentItemChanged, &dialog,
            [&](QListWidgetItem *item)
            {
                if (!item)
                    return;
                auto f = featReference(item->data(Qt::UserRole).toString());
                details.setPlainText(sourcesLast(f["description"].toString() +
                                                 "\n\nAUTOMATIC EFFECTS / CHOICES\n" +
                                                 f["automation"].toString()));
            });
    if (list.count())
        list.setCurrentRow(0);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Add selected");
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    auto feats = array("feats");
    for (int i = 0; i < list.count(); ++i)
        if (list.item(i)->checkState() == Qt::Checked)
        {
            auto f = featReference(list.item(i)->data(Qt::UserRole).toString());
            if (!configureFeat(f, feats))
                return;
            if (f["name"] == "Magic Initiate")
                for (auto v : feats)
                    if (v.toObject()["name"] == f["name"] &&
                        v.toObject()["spellList"] == f["spellList"])
                    {
                        displayError("Magic Initiate must use a different spell list each time.");
                        return;
                    }
            if (f["name"] == "Elemental Adept")
                for (auto v : feats)
                    if (v.toObject()["name"] == f["name"] &&
                        v.toObject()["damageType"] == f["damageType"])
                    {
                        displayError("Each Elemental Adept copy must use a different damage type.");
                        return;
                    }
            feats.append(f);
        }
    put("feats", feats);
    refreshFeats();
}
void editOwnedFeat(bool custom = false)
{
    int r = featTable->currentRow();
    if (!custom && r < 0)
        return;
    auto feats = array("feats");
    auto f =
        custom ? QJsonObject{{"category", "Custom"}, {"active", true}} : feats.at(r).toObject();
    if (!editEffectSource(f, "Feat"))
        return;
    if (custom)
        feats.append(f);
    else
        feats[r] = f;
    put("feats", feats);
    refreshFeats();
}
void editSource(bool add)
{
    int r = sourceTable->currentRow();
    if (!add && r < 0)
        return;
    auto sources = array("effectSources");
    auto s = add ? QJsonObject{{"active", true}} : sources.at(r).toObject();
    if (!editEffectSource(s, "Ability / other bonus"))
        return;
    if (add)
        sources.append(s);
    else
        sources[r] = s;
    put("effectSources", sources);
    refreshSources();
}
void editItemEffects()
{
    int r = inventoryTable->currentRow();
    if (r < 0)
    {
        displayError("Select an inventory item first.");
        return;
    }
    auto items = array("inventory");
    auto item = items.at(r).toObject();
    if (!editEffectSource(item, "Item"))
        return;
    items[r] = item;
    put("inventory", items);
    populateInventory();
}
void addMagicItem()
{
    bool ok = false;
    QString name = QInputDialog::getItem(this, "Add magic item", "Item preset",
                                         {"Cloak of Protection", "Ring of Protection",
                                          "Gauntlets of Ogre Power", "Headband of Intellect",
                                          "Amulet of Health"},
                                         0, false, &ok);
    if (!ok)
        return;
    QJsonArray effects;
    QString description;
    if (name == "Cloak of Protection" || name == "Ring of Protection")
    {
        effects = {effect("ac", 1), effect("save:*", 1)};
        description = "While worn and attuned: +1 AC and +1 to all saving throws. Enable Equipped "
                      "/ active only when those requirements are met.";
    }
    else
    {
        QString stat = name == "Gauntlets of Ogre Power" ? "Strength"
                       : name == "Headband of Intellect" ? "Intelligence"
                                                         : "Constitution";
        effects = {effect("score:" + stat, 19, "flat", "minimum")};
        description = "While worn and attuned: " + stat +
                      " is 19 unless already higher. Enable Equipped / active only when those "
                      "requirements are met.";
    }
    QJsonObject item{{"name", name},
                     {"qty", "1"},
                     {"weight", "0"},
                     {"notes", "Requires attunement; weight can be edited."},
                     {"description", description},
                     {"effects", effects},
                     {"equipped", false}};
    auto items = array("inventory");
    items.append(item);
    put("inventory", items);
    populateInventory();
    inventoryTable->selectRow(items.size() - 1);
}
bool selfTestFullFeats()
{
    const auto snapshot = character;
    QMap<QString, int> counts;
    QSet<QString> names;
    for (auto value : featCatalog)
    {
        auto f = value.toObject();
        if (f["supplemental"].toBool())
            continue;
        QString name = f["name"].toString();
        if (name.isEmpty() || names.contains(name) || f["description"].toString().isEmpty() ||
            f["automation"].toString().isEmpty())
            return false;
        names.insert(name);
        ++counts[f["category"].toString()];
    }
    if (names.size() != 75 || counts.value("Origin") != 10 || counts.value("General") != 43 ||
        counts.value("Fighting Style") != 10 || counts.value("Epic Boon") != 12)
        return false;
    bool rejected = false;
    QTimer answer;
    connect(&answer, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                if (auto *message = qobject_cast<QMessageBox *>(dialog))
                {
                    rejected = true;
                    message->reject();
                    return;
                }
                if (auto *list = dialog->findChild<QListWidget *>("levelUpChoices"))
                {
                    int count = list->property("requiredChoiceCount").toInt();
                    for (int i = 0; i < list->count(); ++i)
                        list->item(i)->setCheckState(i < count ? Qt::Checked : Qt::Unchecked);
                }
                if (dialog->property("abilityBudget").isValid())
                {
                    int remaining = dialog->property("abilityBudget").toInt();
                    for (int i = 0; i < 6; ++i)
                        if (auto *spin =
                                dialog->findChild<QSpinBox *>("abilityPoints" + QString::number(i)))
                        {
                            int value = qMin(remaining, spin->maximum() - spin->minimum());
                            spin->setValue(spin->minimum() + value);
                            remaining -= value;
                        }
                }
                auto *buttons = dialog->findChild<QDialogButtonBox *>();
                if (buttons && buttons->button(QDialogButtonBox::Ok))
                    buttons->button(QDialogButtonBox::Ok)->click();
            });
    answer.start(2);
    auto fail = [&](const QString &name)
    {
        qWarning().noquote() << "Full feat test failed:" << name;
        QFile failure(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (failure.open(QIODevice::WriteOnly | QIODevice::Truncate))
            failure.write(name.toUtf8());
        answer.stop();
        character = snapshot;
        loadWidgets();
        return false;
    };
    for (auto value : featCatalog)
    {
        auto f = value.toObject();
        if (f["supplemental"].toBool())
            continue;
        QString name = f["name"].toString();
        character = blank();
        for (auto a : abilities)
            character[a] = 19;
        character["level"] = 20;
        character["primaryLevel"] = 20;
        character["ac"] = 14;
        character["wearingArmor"] = true;
        character["armorDexMode"] = "Maximum +2";
        loadWidgets();
        if (!configureFeat(f) || rejected)
            return fail(name + " choices");
        put("feats", QJsonArray{f});
        if (f["category"] == "General" || f["category"] == "Epic Boon")
        {
            bool increased = false;
            for (int i = 0; i < 6; ++i)
                increased |= effectiveScore(i) > 19;
            if (!increased)
                return fail(name + " ability increase");
        }
        if (name == "Speedy" && effectiveSpeed() != 40)
            return fail(name);
        if (name == "Boon of Speed" && effectiveSpeed() != 60)
            return fail(name);
        if (name == "Boon of Fortitude" && maximumHp() != 51)
            return fail(name);
        if (name == "Resilient" && saveBonus(1) != 11)
            return fail(name);
        if (name == "Durable" &&
            (effectAdvantage({"save:*", "deathSave"}) != 1 || effectAdvantage(saveTargets(2)) != 0))
            return fail(name);
        if (name == "War Caster")
        {
            if (effectAdvantage({"save:*", "save:Constitution", "concentration"}) != 1 ||
                effectAdvantage(saveTargets(2)) != 0)
                return fail(name);
            auto roll = d20("Concentration test", saveBonus(2),
                            {"save:*", "save:Constitution", "concentration"});
            if (!roll.detail.startsWith("advantage"))
                return fail(name + " roll");
        }
        if (name == "Boon of Skill")
            for (int i = 0; i < 18; ++i)
                if (skillBonus(i) < ability(skillAbilities[i]) + pb())
                    return fail(name);
        if (name == "Keen Mind" && skillBonus(2) != ability(3) + pb())
            return fail(name);
        if (name == "Observant" && skillBonus(6) != ability(4) + pb())
            return fail(name);
        if (name == "Skill Expert" && skillBonus(0) != ability(1) + 2 * pb())
            return fail(name);
        if (name == "Medium Armor Master")
        {
            if (armorClass() != 15)
                return fail(name);
            put("wearingArmor", false);
            if (armorClass() != 14)
                return fail(name + " armor condition");
        }
        if (name == "Dueling")
        {
            QJsonObject weapon{{"name", "Longsword"},
                               {"ability", 0},
                               {"properties", "Versatile (1d10)"},
                               {"dueling", true}};
            if (weaponBonus(weapon, true) != 6)
                return fail(name);
            weapon["twoHanded"] = true;
            if (weaponBonus(weapon, true) != 4)
                return fail(name + " two hands");
        }
        if (name == "Thrown Weapon Fighting")
        {
            QJsonObject weapon{{"name", "Javelin"},
                               {"ability", 0},
                               {"properties", "Thrown (30/120)"},
                               {"thrown", true}};
            if (weaponBonus(weapon, true) != 6)
                return fail(name);
            weapon["thrown"] = false;
            if (weaponBonus(weapon, true) != 4)
                return fail(name + " melee use");
        }
        if (name == "Great Weapon Master")
        {
            QJsonObject weapon{
                {"name", "Greatsword"}, {"ability", 0}, {"properties", "Heavy, Two-Handed"}};
            if (weaponBonus(weapon, true) != 11)
                return fail(name);
            put("attackAction", false);
            if (weaponBonus(weapon, true) != 5)
                return fail(name + " reaction");
        }
        if (name == "Martial Weapon Training")
        {
            QJsonObject weapon{{"name", "Longsword"}, {"ability", 0}, {"trained", false}};
            if (weaponBonus(weapon, false) != 11)
                return fail(name);
        }
        f["active"] = false;
        put("feats", QJsonArray{f});
        if (effectiveSpeed() != 30 || maximumHp() != 11 || armorClass() != 14)
            return fail(name + " removal");
        for (int i = 0; i < 6; ++i)
            if (effectiveScore(i) != 19)
                return fail(name + " base score preservation");
    }
    // A new skill proficiency selected earlier in the same batch qualifies for expertise.
    character = blank();
    loadWidgets();
    auto knowledge = featReference("Keen Mind");
    QJsonArray pending{
        QJsonObject{{"id", "pending-training"},
                    {"active", true},
                    {"effects", QJsonArray{effect("skill:Arcana", 0, "flat", "proficiency")}}}};
    if (!configureFeat(knowledge, pending))
        return fail("pending skill choices");
    if (knowledge["effects"].toArray().last().toObject()["op"] != "expertise")
        return fail("pending expertise");
    answer.stop();
    QFile::remove(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
    character = snapshot;
    loadWidgets();
    return true;
}
bool selfTestEffects()
{
    auto snapshot = character;
    character = blank();
    character["Strength"] = 14;
    character["Dexterity"] = 14;
    character["Constitution"] = 12;
    character["Intelligence"] = 16;
    character["level"] = 5;
    character["primaryLevel"] = 3;
    character["multiclasses"] = QJsonArray{QJsonObject{{"name", "Wizard"}, {"level", 2}}};
    character["hpMax"] = 30;
    character["hpCurrent"] = 20;
    character["ac"] = 14;
    character["speed"] = 30;
    loadWidgets();
    if (featCatalog.size() != 264)
        return false;
    bool picked = false;
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *list = dialog->findChild<QListWidget *>();
            if (!list)
            {
                dialog->reject();
                return;
            }
            for (int i = 0; i < list->count(); ++i)
                if (QStringList{"Alert", "Tough", "Defense", "Archery"}.contains(
                        list->item(i)->data(Qt::UserRole).toString()))
                    list->item(i)->setCheckState(Qt::Checked);
            dialog->grab().save(QCoreApplication::applicationDirPath() +
                                "/preview-feat-picker.png");
            picked = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    selectFeats();
    if (!picked || array("feats").size() != 4 || initiativeBonus() != 5 || maximumHp() != 40 ||
        armorClass() != 14)
        return false;
    armorCheck->setChecked(true);
    if (armorClass() != 15)
        return false;
    QJsonObject cloak{{"name", "Protection"},
                      {"qty", "1"},
                      {"equipped", true},
                      {"effects", QJsonArray{effect("ac", 1), effect("save:*", 1)}}};
    QJsonObject amulet{
        {"name", "Health"},
        {"qty", "1"},
        {"equipped", true},
        {"effects", QJsonArray{effect("score:Constitution", 19, "flat", "minimum")}}};
    put("inventory", QJsonArray{cloak, amulet});
    populateInventory();
    if (effectiveScore(2) != 19 || maximumHp() != 55 || armorClass() != 16 ||
        number("hpCurrent") != 20)
        return false;
    QJsonArray effects{effect("score:Dexterity", 2),
                       effect("speed", 10),
                       effect("weaponAttack", 1),
                       effect("weaponDamage", 2),
                       effect("spellAttack", 1),
                       effect("spellDC", 2),
                       effect("spellHealing", 2),
                       effect("save:Constitution", 0, "flat", "proficiency"),
                       effect("skill:Arcana", 0, "flat", "expertise")};
    effects.append(QJsonObject{{"target", "save:*"}, {"op", "dice"}, {"value", "1d4"}});
    QJsonObject source{{"name", "Training and blessing"}, {"active", true}, {"effects", effects}};
    put("effectSources", QJsonArray{source});
    if (armorClass() != 17 || effectiveSpeed() != 40 || initiativeBonus() != 6 ||
        saveBonus(2) != 8 || skillBonus(2) != 9 || spellFormula("Cure Wounds", 1) != "2d8+3+2")
        return false;
    QJsonObject bow{{"name", "Shortbow"}, {"ability", 1}, {"magic", 1}, {"trained", true}};
    QJsonObject sword{{"name", "Longsword"}, {"ability", 0}, {"magic", 1}, {"trained", true}};
    if (weaponBonus(bow, false) != 10 || weaponBonus(bow, true) != 6 ||
        weaponBonus(sword, false) != 7)
        return false;
    auto saveRoll = d20("Effect test", saveBonus(2), saveTargets(2));
    if (!saveRoll.detail.contains("1d4") || saveRoll.total < 10 || saveRoll.total > 32)
        return false;
    inventoryTable->item(0, 1)->setText("2");
    if (array("inventory").first().toObject()["effects"].toArray().size() != 2)
        return false;
    inventoryTable->item(0, 4)->setCheckState(Qt::Unchecked);
    if (armorClass() != 16 || saveBonus(2) != 7)
        return false;
    // Removing sources restores base stats without subtracting from saved scores.
    put("effectSources", QJsonArray{});
    put("inventory", QJsonArray{});
    put("feats", QJsonArray{});
    if (effectiveScore(1) != 14 || effectiveScore(2) != 12 || armorClass() != 14 ||
        maximumHp() != 30 || number("Strength") != 14)
        return false;
    QJsonObject asi{{"name", "ASI"},
                    {"effects", QJsonArray{effect("score:Strength", 2, "flat", "bonus", 20)}}};
    character["Strength"] = 19;
    put("feats", QJsonArray{asi});
    if (effectiveScore(0) != 20)
        return false;
    put("feats", QJsonArray{});
    put("effectSources",
        QJsonArray{QJsonObject{{"name", "Penalty"},
                               {"effects", QJsonArray{effect("score:Strength", -2)}}}});
    if (effectiveScore(0) != 17)
        return false;
    put("effectSources", QJsonArray{});
    auto configuredAsi = featReference("Ability Score Improvement");
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *points = dialog->findChild<QSpinBox *>("abilityPoints1");
            if (!points)
            {
                dialog->reject();
                return;
            }
            points->setValue(points->minimum() + 2);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    if (!configureFeat(configuredAsi))
        return false;
    put("feats", QJsonArray{configuredAsi});
    if (effectiveScore(1) != 16 || number("Dexterity") != 14)
        return false;
    put("feats", QJsonArray{});
    setNumber("Charisma", 14);
    put("effectSources",
        QJsonArray{QJsonObject{{"name", "Ability scaling"},
                               {"effects", QJsonArray{effect("save:*", 1, "Charisma")}}}});
    if (saveBonus(4) != 2)
        return false;
    setNumber("Charisma", 16);
    if (saveBonus(4) != 3)
        return false;
    setNumber("Charisma", 8);
    // Persist feat, item and ability effects across character switches.
    character["Strength"] = 14;
    put("effectSources", QJsonArray{source});
    put("inventory", QJsonArray{cloak, amulet});
    auto alert = featReference("Alert");
    if (!configureFeat(alert))
        return false;
    put("feats", QJsonArray{alert});
    refreshFeats();
    populateInventory();
    save();
    QString id = current;
    newCharacter();
    openCharacter(id);
    if (array("feats").size() != 1 || array("effectSources").size() != 1 ||
        array("inventory").size() != 2 || effectiveScore(2) != 19 || initiativeBonus() != 6)
        return false;
    // Exercise the effects editor and verify cancellation does not change the source.
    QJsonObject draft = source;
    bool edited = false;
    QTimer::singleShot(
        0, this,
        [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *table = dialog->findChild<QTableWidget *>();
            if (!table || table->rowCount() != 10)
            {
                dialog->reject();
                return;
            }
            qobject_cast<QLineEdit *>(table->cellWidget(1, 2))->setText("15");
            dialog->grab().save(QCoreApplication::applicationDirPath() +
                                "/preview-effect-editor.png");
            edited = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
    if (!editEffectSource(draft, "Ability / other bonus") || !edited ||
        draft["effects"].toArray().at(1).toObject()["value"].toInt() != 15)
        return false;
    auto unchanged = draft;
    QTimer::singleShot(0, this,
                       []
                       {
                           auto *dialog =
                               qobject_cast<QDialog *>(QApplication::activeModalWidget());
                           if (dialog)
                               dialog->reject();
                       });
    if (editEffectSource(draft, "Ability / other bonus") || draft != unchanged)
        return false;
    tabs->setCurrentWidget(featPage);
    QApplication::processEvents();
    grab().save(QCoreApplication::applicationDirPath() + "/preview-effects.png");
    setNumber("hpCurrent", 0);
    setNumber("deathSuccess", 0);
    setNumber("deathFails", 0);
    applyDeathSave(9, 10);
    if (number("deathSuccess") != 1)
        return false;
    applyDeathSave(1, 25);
    if (number("deathFails") != 2)
        return false;
    applyDeathSave(20, 0);
    if (number("hpCurrent") != 1)
        return false;
    character = snapshot;
    loadWidgets();
    return true;
}
