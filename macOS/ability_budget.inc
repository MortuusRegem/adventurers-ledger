bool selfTestAbilityBudgets()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](QString reason)
    {
        QFile f(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (f.open(QIODevice::WriteOnly))
            f.write(("Ability budgets: " + reason).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    int subclassesChecked = 0;
    auto descriptions = subclassDescriptions();
    for (auto cls : classes)
        for (auto name : subclasses.value(cls))
        {
            if (descriptions.value(name).size() < 50)
                return fail("missing subclass description: " + name);
            ++subclassesChecked;
        }
    if (subclassesChecked != 130)
        return fail("subclass count");
    character = blank();
    character["abilityBudgetManaged"] = true;
    character["abilityPool"] = QJsonArray{15, 14, 13, 12, 10, 8};
    character["abilityMethod"] = "Standard array";
    loadWidgets();
    setNumber("Strength", 20);
    if (number("Strength") != 15 || !abilityBudgetProblem().isEmpty())
        return fail("base edit bypassed assigned pool");
    auto *strength = qobject_cast<QSpinBox *>(fields["Strength"]),
         *dexterity = qobject_cast<QSpinBox *>(fields["Dexterity"]);
    if (strength->isReadOnly() || strength->buttonSymbols() != QAbstractSpinBox::UpDownArrows)
        return fail("starting score arrows unavailable");
    strength->stepDown();
    if (usedAbilityTotal() != 71 || dexterity->maximum() != 15)
        return fail("lowering did not free a point");
    dexterity->stepUp();
    if (usedAbilityTotal() != 72 || !abilityBudgetProblem().isEmpty())
        return fail("arrow reassignment failed");
    int oldDex = number("Dexterity");
    dexterity->stepUp();
    if (number("Dexterity") != oldDex)
        return fail("arrow overspent full budget");
    character["Strength"] = 15;
    character["Dexterity"] = 14;
    loadWidgets();
    bool scoreArrows = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!d)
                return;
            auto *str = d->findChild<QSpinBox *>("startingScore0"),
                 *dex = d->findChild<QSpinBox *>("startingScore1");
            if (!str || !dex)
            {
                d->reject();
                return;
            }
            str->stepDown();
            dex->stepUp();
            auto *counter = d->findChild<QLabel *>("scoreBudgetCounter");
            scoreArrows = counter &&
                          counter->text().contains("TOTAL: 72    USED: 72    REMAINING: 0") &&
                          dex->maximum() == dex->value();
            d->grab().save(QCoreApplication::applicationDirPath() + "/preview-score-arrows.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
    if (!assignAbilityPool() || !scoreArrows || number("Strength") != 14 ||
        number("Dexterity") != 15)
        return fail("score dialog arrows and counter");
    character["Strength"] = 15;
    character["Dexterity"] = 14;
    loadWidgets();
    character["Strength"] = 16;
    if (abilityBudgetProblem().isEmpty())
        return fail("overallocated pool accepted");
    character["Strength"] = 15;
    character["Strength"] = 17;
    character["Dexterity"] = 15;
    character["backgroundIncreases"] = QJsonObject{{"Strength", 2}, {"Dexterity", 1}};
    if (!abilityBudgetProblem().isEmpty())
        return fail("legal background bonus rejected");
    character["Strength"] = 18;
    if (abilityBudgetProblem().isEmpty())
        return fail("extra background point accepted");
    character = blank();
    character["abilityBudgetManaged"] = true;
    character["abilityMethod"] = "Rolled 4d6, drop lowest";
    character["abilityPool"] = QJsonArray{18, 16, 14, 12, 9, 6};
    int rolledValues[] = {18, 16, 14, 12, 9, 6};
    for (int i = 0; i < 6; ++i)
        character[abilities[i]] = rolledValues[i];
    loadWidgets();
    if (assignedAbilityTotal() != 75 || !abilityBudgetProblem().isEmpty())
        return fail("rolled total replaced by standard array");
    auto *rolledStr = qobject_cast<QSpinBox *>(fields["Strength"]);
    auto *rolledDex = qobject_cast<QSpinBox *>(fields["Dexterity"]);
    rolledStr->stepDown();
    rolledDex->stepUp();
    if (usedAbilityTotal() != 75 || number("Strength") != 17 || number("Dexterity") != 17 ||
        !abilityBudgetProblem().isEmpty())
        return fail("rolled points cannot be redistributed");
    rolledDex->stepUp();
    if (number("Dexterity") != 17)
        return fail("rolled total overspent");
    character["backgroundIncreases"] = QJsonObject{{"Strength", 2}, {"Dexterity", 1}};
    character["Strength"] = 19;
    character["Dexterity"] = 18;
    loadWidgets();
    if (usedAbilityTotal() != 75 || !abilityBudgetProblem().isEmpty())
        return fail("rolled background points counted against budget");
    character["Strength"] = 21;
    if (abilityBudgetProblem().isEmpty())
        return fail("rolled score cap bypassed");
    resetAbilityPool(false);
    if (assignedAbilityTotal() != 72 || usesRolledAbilityBudget() ||
        !abilityBudgetProblem().isEmpty())
        return fail("switching back to standard array failed");
    resetAbilityPool(true);
    if (!usesRolledAbilityBudget() || !abilityBudgetProblem().isEmpty())
        return fail("rolling new scores failed");
    character = blank();
    character["Strength"] = 19;
    character["Dexterity"] = 19;
    loadWidgets();
    bool allocationChecked = false;
    QTimer::singleShot(
        30, this,
        [&]
        {
            auto *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!d)
                return;
            auto *str = d->findChild<QSpinBox *>("abilityPoints0"),
                 *dex = d->findChild<QSpinBox *>("abilityPoints1");
            if (!str || !dex)
            {
                d->reject();
                return;
            }
            str->setValue(21);
            if (str->value() != 20)
            {
                d->reject();
                return;
            }
            dex->setValue(20);
            if (d->findChild<QSpinBox *>("abilityPoints2")->maximum() !=
                d->findChild<QSpinBox *>("abilityPoints2")->minimum())
            {
                d->reject();
                return;
            }
            allocationChecked = true;
            d->grab().save(QCoreApplication::applicationDirPath() + "/preview-ability-points.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    auto feat = featReference("Ability Score Improvement");
    if (!configureFeat(feat) || !allocationChecked || feat["abilitySpent"].toInt() != 2 ||
        feat["abilityAllocated"].toObject()["Strength"].toInt() != 1 ||
        feat["abilityAllocated"].toObject()["Dexterity"].toInt() != 1)
        return fail("ASI budget or cap");
    character["feats"] = QJsonArray{feat};
    if (permanentAbilityScore(0, array("feats")) != 20)
        return fail("pending owned feat counted twice");
    character = blank();
    character["levelUpDraft"] = true;
    character["manageClassProgression"] = true;
    loadWidgets();
    setNumber("Strength", 30);
    if (number("Strength") != 15)
        return fail("free level-up base points allowed");
    character = blank();
    character["primaryLevel"] = 3;
    character["level"] = 3;
    QJsonArray tasks;
    addAdvanceTask(tasks, "Fighter", 3, "subclass", "Choose Fighter subclass");
    character["advancementTasks"] = tasks;
    loadWidgets();
    bool described = false;
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
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text() == "Champion")
                {
                    list->setCurrentRow(i);
                    list->item(i)->setCheckState(Qt::Checked);
                }
            auto *detail = d->findChild<QPlainTextEdit *>();
            described = detail && detail->toPlainText().contains("critical hits");
            d->grab().save(QCoreApplication::applicationDirPath() +
                           "/preview-subclass-descriptions.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
    if (!resolveAdvanceTask(0) || !described || text("subclass") != "Champion")
        return fail("subclass chooser details");
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}
bool baseAbilitiesProtected() const
{
    return character["abilitiesLocked"].toBool() ||
           (character["levelUpDraft"].toBool() && character["manageClassProgression"].toBool());
}
int assignedAbilityTotal() const
{
    int total = 0;
    for (auto v : array("abilityPool"))
        total += v.toInt();
    return total;
}
int usedAbilityTotal() const
{
    int used = 0;
    auto bg = character["backgroundIncreases"].toObject();
    for (auto a : abilities)
        used += number(a) - bg[a].toInt();
    return used;
}
bool usesRolledAbilityBudget() const
{
    return text("abilityMethod").startsWith("Rolled 4d6");
}
bool allowedBaseScore(const QString &key, int value) const
{
    auto pool = array("abilityPool");
    if (!character["abilityBudgetManaged"].toBool() || pool.size() != 6)
        return true;
    int minimum = 30, maximum = 0;
    for (auto v : pool)
    {
        minimum = qMin(minimum, v.toInt());
        maximum = qMax(maximum, v.toInt());
    }
    if (usesRolledAbilityBudget())
    {
        minimum = 3;
        maximum = 18;
    }
    int bonus = character["backgroundIncreases"].toObject()[key].toInt();
    return value >= minimum + bonus && value <= qMin(20, maximum + bonus) &&
           usedAbilityTotal() - number(key) + value <= assignedAbilityTotal();
}
QString abilityBudgetProblem() const
{
    if (!character["abilityBudgetManaged"].toBool())
        return {};
    auto pool = array("abilityPool");
    if (pool.size() != 6)
        return "Assign all six starting scores.";
    QList<int> assigned, available;
    auto background = character["backgroundIncreases"].toObject();
    int backgroundSpent = 0;
    for (auto key : background.keys())
    {
        int value = background[key].toInt();
        if (!abilities.contains(key) || value < 0 || value > 2)
            return "Invalid background ability allocation.";
        backgroundSpent += value;
    }
    if (backgroundSpent > 3)
        return "Background increases cannot spend more than 3 points.";
    for (int i = 0; i < 6; ++i)
    {
        available << pool.at(i).toInt();
        assigned << number(abilities[i]) - background[abilities[i]].toInt();
    }
    if (usesRolledAbilityBudget())
    {
        for (int value : assigned)
            if (value < 3 || value > 18)
                return "Rolled starting scores must be between 3 and 18 before background "
                       "increases.";
        return usedAbilityTotal() == assignedAbilityTotal()
                   ? QString()
                   : "Use the full rolled point budget before locking abilities.";
    }
    std::sort(assigned.begin(), assigned.end());
    std::sort(available.begin(), available.end());
    return assigned == available ? QString()
                                 : "Starting abilities must use exactly the six assigned scores. "
                                   "Background and earned feat increases are separate.";
}
void resetAbilityPool(bool rolled)
{
    if (character["abilitiesLocked"].toBool() || character["levelUpDraft"].toBool())
        return;
    QJsonArray pool;
    int standard[] = {15, 14, 13, 12, 10, 8};
    for (int i = 0; i < 6; ++i)
    {
        int value = standard[i];
        if (rolled)
        {
            QList<int> dice;
            for (int n = 0; n < 4; ++n)
                dice << die(6);
            std::sort(dice.begin(), dice.end());
            value = dice[1] + dice[2] + dice[3];
        }
        pool.append(value);
        character[abilities[i]] = value;
    }
    character["abilityBudgetManaged"] = true;
    character["abilityPool"] = pool;
    character["abilityMethod"] = rolled ? "Rolled 4d6, drop lowest" : "Standard array";
    character["backgroundIncreases"] = QJsonObject{};
    character["backgroundApplied"] = false;
    loadWidgets();
    put("abilityPool", pool);
}
bool assignAbilityPool()
{
    if (character["abilitiesLocked"].toBool() || character["levelUpDraft"].toBool())
    {
        displayError("Starting scores are fixed. Use the earned feat or ASI choice to allocate "
                     "level-up increases.");
        return false;
    }
    auto pool = array("abilityPool");
    if (pool.size() != 6)
    {
        pool = {};
        for (auto a : abilities)
            pool.append(number(a));
    }
    auto background = character["backgroundIncreases"].toObject();
    int minimum = 30, maximum = 0, total = 0;
    QStringList originals;
    for (auto v : pool)
    {
        minimum = qMin(minimum, v.toInt());
        maximum = qMax(maximum, v.toInt());
        total += v.toInt();
        originals << QString::number(v.toInt());
    }
    if (usesRolledAbilityBudget())
    {
        minimum = 3;
        maximum = 18;
    }
    QDialog d(this);
    d.setWindowTitle("Adjust ability scores");
    auto *layout = new QVBoxLayout(&d);
    auto *status = new QLabel;
    status->setWordWrap(true);
    status->setObjectName("scoreBudgetCounter");
    layout->addWidget(status);
    auto *form = new QFormLayout;
    QSpinBox *scores[6];
    for (int i = 0; i < 6; ++i)
    {
        scores[i] = new QSpinBox;
        scores[i]->setObjectName("startingScore" + QString::number(i));
        scores[i]->setRange(minimum + background[abilities[i]].toInt(),
                            qMin(20, maximum + background[abilities[i]].toInt()));
        scores[i]->setValue(number(abilities[i]));
        form->addRow(abilities[i], scores[i]);
    }
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    auto update = [&]
    {
        int used = 0;
        QList<int> actual, wanted;
        for (int i = 0; i < 6; ++i)
        {
            used += scores[i]->value() - background[abilities[i]].toInt();
            actual << scores[i]->value() - background[abilities[i]].toInt();
            wanted << pool.at(i).toInt();
        }
        int remaining = qMax(0, total - used);
        for (int i = 0; i < 6; ++i)
        {
            QSignalBlocker block(scores[i]);
            scores[i]->setMaximum(qMin(qMin(20, maximum + background[abilities[i]].toInt()),
                                       scores[i]->value() + remaining));
        }
        std::sort(actual.begin(), actual.end());
        std::sort(wanted.begin(), wanted.end());
        status->setText(QString("TOTAL: %1    USED: %2    REMAINING: %3\n%4\nStarting scores: %5. "
                                "Background increases stay attached to their selected abilities.")
                            .arg(total)
                            .arg(used)
                            .arg(remaining)
                            .arg(remaining == 0 ? "Budget full — lower a score to free points."
                                                : "Use the arrows to move the available points.")
                            .arg(originals.join(", ")));
        buttons->button(QDialogButtonBox::Save)
            ->setEnabled(usesRolledAbilityBudget() ? used == total : actual == wanted);
    };
    for (auto *score : scores)
        connect(score, &QSpinBox::valueChanged, &d, update);
    update();
    connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted)
        return false;
    character["abilityPool"] = pool;
    character["abilityBudgetManaged"] = true;
    for (int i = 0; i < 6; ++i)
        character[abilities[i]] = scores[i]->value();
    loadWidgets();
    put("abilityBudgetManaged", true);
    return true;
}

int permanentAbilityScore(int i, const QJsonArray &pending = {}) const
{
    int score = number(abilities[i]);
    QJsonArray effects = activeEffects();
    for (auto v : pending)
        if (!array("feats").contains(v))
            for (auto e : v.toObject()["effects"].toArray())
                effects.append(e);
    for (auto v : effects)
    {
        auto e = v.toObject();
        if (e["target"].toString() == "score:" + abilities[i] &&
            e["op"].toString("bonus") == "bonus")
        {
            int value = effectValue(e);
            score =
                value > 0 ? qMax(score, qMin(e["cap"].toInt(30), score + value)) : score + value;
        }
    }
    return score;
}
bool allocateFeatAbilities(QString name, QStringList allowed, int budget, int cap,
                           const QJsonArray &pending, QJsonObject &allocation)
{
    QDialog d(this);
    d.setWindowTitle(name + " — ability points");
    d.setProperty("abilityBudget", budget);
    auto *layout = new QVBoxLayout(&d);
    auto *status = new QLabel;
    layout->addWidget(status);
    auto *form = new QFormLayout;
    QSpinBox *points[6];
    int capacity = 0;
    for (int i = 0; i < 6; ++i)
    {
        int room = allowed.contains(abilities[i])
                       ? qMax(0, qMin(budget, cap - permanentAbilityScore(i, pending)))
                       : 0;
        capacity += room;
        points[i] = new QSpinBox;
        points[i]->setObjectName("abilityPoints" + QString::number(i));
        points[i]->setRange(permanentAbilityScore(i, pending),
                            permanentAbilityScore(i, pending) + room);
        points[i]->setEnabled(room > 0);
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(points[i]);
        rowLayout->addWidget(
            button("Details", [this, i]
                   { showRuleDescription(abilities[i], basicChoiceDescription(abilities[i])); }));
        form->addRow(
            abilities[i] +
                QString(" (current %1, cap %2)").arg(permanentAbilityScore(i, pending)).arg(cap),
            row);
    }
    layout->addLayout(form);
    if (capacity < budget)
    {
        displayError("There is not enough room below the score cap to spend this choice's ability "
                     "points. Choose another eligible feat.");
        return false;
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    bool changing = false;
    auto update = [&]
    {
        if (changing)
            return;
        changing = true;
        int spent = 0;
        for (auto *spin : points)
            spent += spin->value() - spin->minimum();
        for (int i = 0; i < 6; ++i)
        {
            QSignalBlocker block(points[i]);
            int room = allowed.contains(abilities[i])
                           ? qMax(0, qMin(budget, cap - permanentAbilityScore(i, pending)))
                           : 0;
            points[i]->setMaximum(
                qMin(points[i]->minimum() + room, points[i]->value() + qMax(0, budget - spent)));
        }
        status->setText(QString("TOTAL: %1    USED: %2    REMAINING: %3\nUse the score arrows. At "
                                "the maximum, lower a score to free points.")
                            .arg(budget)
                            .arg(spent)
                            .arg(budget - spent));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(spent == budget);
        changing = false;
    };
    for (auto *spin : points)
        connect(spin, &QSpinBox::valueChanged, &d, update);
    update();
    connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted)
        return false;
    for (int i = 0; i < 6; ++i)
        if (points[i]->value() > points[i]->minimum())
            allocation[abilities[i]] = points[i]->value() - points[i]->minimum();
    return true;
}
void refreshAbilityBudget()
{
    if (!abilityBudgetStatus)
        return;
    QString budgetSummary;
    if (character["abilityBudgetManaged"].toBool())
    {
        int total = assignedAbilityTotal(), used = usedAbilityTotal(),
            remaining = qMax(0, total - used), background = 0;
        auto bg = character["backgroundIncreases"].toObject();
        for (auto v : bg)
            background += v.toInt();
        auto pool = array("abilityPool");
        int minimum = 30, maximum = 0;
        QStringList originals;
        for (auto v : pool)
        {
            minimum = qMin(minimum, v.toInt());
            maximum = qMax(maximum, v.toInt());
            originals << QString::number(v.toInt());
        }
        if (usesRolledAbilityBudget())
        {
            minimum = 3;
            maximum = 18;
        }
        if (pool.size() == 6 && !baseAbilitiesProtected())
            for (auto a : abilities)
                if (auto *spin = qobject_cast<QSpinBox *>(fields.value(a)))
                {
                    QSignalBlocker block(spin);
                    spin->setRange(minimum + bg[a].toInt(),
                                   qMin(qMin(20, maximum + bg[a].toInt()), number(a) + remaining));
                }
        budgetSummary =
            QString("TOTAL: %1    USED: %2    REMAINING: %3\n%4 · starting scores %5 · background "
                    "points %6 / 3. %7")
                .arg(total)
                .arg(used)
                .arg(remaining)
                .arg(text("abilityMethod"), originals.join(", "))
                .arg(background)
                .arg(remaining == 0
                         ? "Lower a score to free points before increasing another."
                         : (usesRolledAbilityBudget() ? "Adjust scores with the arrows; use the "
                                                        "full rolled total (3-18 per score)."
                                                      : "Adjust scores with the arrows; finish "
                                                        "using the original six results."));
    }
    if (character["levelUpDraft"].toBool())
        budgetSummary =
            "Level-up ability points are assigned inside the earned feat/ASI choice. Base "
            "scores cannot be increased directly.";
    for (auto v : array("feats"))
    {
        auto f = v.toObject();
        if (f.contains("abilityBudget"))
            budgetSummary += "\n" + f["name"].toString() +
                             QString(": %1 / %2 ability points used")
                                 .arg(f["abilitySpent"].toInt())
                                 .arg(f["abilityBudget"].toInt());
    }
    abilityBudgetStatus->setText(budgetSummary);
}
