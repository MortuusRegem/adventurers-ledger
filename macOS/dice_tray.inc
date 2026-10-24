void refreshTrayControls()
{
    int count = 0;
    for (auto *spin : trayCounts)
        count += spin->value();
    int remaining = qMax(0, 40 - count);
    for (auto *spin : trayCounts)
    {
        QSignalBlocker block(spin);
        spin->setMaximum(spin->value() + remaining);
        spin->setEnabled(!trayFinish.isActive());
    }
    trayModifier->setEnabled(!trayFinish.isActive());
    trayRollButton->setEnabled(count > 0 && !trayFinish.isActive());
    trayClearButton->setEnabled(!trayFinish.isActive());
    trayCounter->setText(QString("Dice selected: %1 / 40").arg(count));
}
void removeTrayDice()
{
    trayDice.clear();
    while (auto *item = trayGrid->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}
void clearDiceTray()
{
    if (trayFinish.isActive())
        return;
    for (auto *spin : trayCounts)
    {
        QSignalBlocker block(spin);
        spin->setValue(0);
    }
    for (auto it = trayPreviews.begin(); it != trayPreviews.end(); ++it)
        it.value()->preview(it.key());
    trayModifier->setValue(0);
    removeTrayDice();
    lastTrayDice = {};
    lastTrayTotal = 0;
    lastTrayDetail.clear();
    trayResult->setText("Choose your dice, then roll the tray.");
    refreshTrayControls();
}
void rollDiceTray()
{
    if (trayFinish.isActive())
        return;
    int count = 0;
    for (auto *spin : trayCounts)
        count += spin->value();
    if (count < 1 || count > 40)
        return;
    removeTrayDice();
    lastTrayDice = {};
    lastTrayTotal = trayModifier->value();
    QStringList expression, groups;
    int position = 0;
    int columns = qBound(1, trayGrid->parentWidget()->width() / 108, 12);
    for (auto it = trayCounts.begin(); it != trayCounts.end(); ++it)
    {
        int sides = it.key(), quantity = it.value()->value();
        if (!quantity)
            continue;
        expression << QString("%1d%2").arg(quantity).arg(sides);
        QStringList faces;
        int subtotal = 0;
        for (int i = 0; i < quantity; ++i)
        {
            int face = die(sides);
            subtotal += face;
            faces << QString::number(face);
            lastTrayDice.append(QJsonObject{{"sides", sides}, {"face", face}});
            auto *tile = new QWidget;
            auto *layout = new QVBoxLayout(tile);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            auto *label = new QLabel("d" + QString::number(sides));
            label->setAlignment(Qt::AlignCenter);
            layout->addWidget(label);
            auto *visual = new RollingDie;
            layout->addWidget(visual);
            Roll result;
            result.sides = sides;
            result.face = face;
            result.total = face;
            result.detail = QString("d%1 [%2]").arg(sides).arg(face);
            visual->animate(result);
            if (i == 0)
                trayPreviews[sides]->animate(result);
            trayDice << visual;
            trayGrid->addWidget(tile, position / columns, position % columns,
                                Qt::AlignLeft | Qt::AlignTop);
            ++position;
        }
        lastTrayTotal += subtotal;
        groups << QString("%1d%2 [%3] = %4")
                      .arg(quantity)
                      .arg(sides)
                      .arg(faces.join(", "))
                      .arg(subtotal);
    }
    QString modifierText = trayModifier->value() ? sign(trayModifier->value()) : QString();
    lastTrayDetail =
        expression.join(" + ") + modifierText + "\n" + groups.join("; ") +
        (trayModifier->value() ? QString("; modifier %1").arg(sign(trayModifier->value()))
                               : QString()) +
        QString("\nTOTAL: %1").arg(lastTrayTotal);
    trayResult->setText(QString("Rolling %1 dice…").arg(count));
    trayFinish.start(1100);
    refreshTrayControls();
    log("Dice tray", lastTrayDetail);
}
void buildDiceTray(QVBoxLayout *body)
{
    auto *box = new QGroupBox("VIRTUAL DICE TRAY");
    auto *layout = new QVBoxLayout(box);
    auto *note = new QLabel("Choose how many of each die to roll together. These are raw dice "
                            "rolls with the modifier below.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *selection = new QGridLayout;
    int column = 0;
    for (int sides : {4, 6, 8, 10, 12, 20, 100})
    {
        auto *card = new QWidget;
        card->setMinimumSize(110, 130);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(6, 0, 6, 0);
        cardLayout->setSpacing(4);
        auto *visual = new RollingDie;
        visual->setObjectName("trayPreview" + QString::number(sides));
        visual->preview(sides);
        trayPreviews[sides] = visual;
        cardLayout->addWidget(visual, 0, Qt::AlignHCenter);
        auto *quantity = new QHBoxLayout;
        quantity->addWidget(new QLabel("Dice"));
        auto *spin = new QSpinBox;
        spin->setObjectName("trayCount" + QString::number(sides));
        spin->setAccessibleName("Number of d" + QString::number(sides) + " dice");
        spin->setRange(0, 40);
        spin->setFixedWidth(65);
        trayCounts[sides] = spin;
        quantity->addWidget(spin);
        quantity->setAlignment(Qt::AlignHCenter);
        cardLayout->addLayout(quantity);
        selection->addWidget(card, 0, column);
        ++column;
        connect(spin, &QSpinBox::valueChanged, this, [this] { refreshTrayControls(); });
    }
    layout->addLayout(selection);
    auto *actions = new QHBoxLayout;
    actions->addWidget(new QLabel("Modifier"));
    trayModifier = new QSpinBox;
    trayModifier->setObjectName("trayModifier");
    trayModifier->setRange(-999, 999);
    actions->addWidget(trayModifier);
    trayRollButton = button("Roll tray", [this] { rollDiceTray(); });
    trayRollButton->setObjectName("rollDiceTray");
    actions->addWidget(trayRollButton);
    trayClearButton = button("Clear tray", [this] { clearDiceTray(); });
    actions->addWidget(trayClearButton);
    trayCounter = new QLabel;
    actions->addWidget(trayCounter);
    actions->addStretch();
    layout->addLayout(actions);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(185);
    scroll->setMaximumHeight(300);
    auto *canvas = new QWidget;
    trayGrid = new QGridLayout(canvas);
    trayGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    trayGrid->setSpacing(8);
    scroll->setWidget(canvas);
    layout->addWidget(scroll);
    trayResult = new QLabel("Choose your dice, then roll the tray.");
    trayResult->setObjectName("trayResult");
    trayResult->setWordWrap(true);
    trayResult->setMinimumHeight(60);
    trayResult->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    trayResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(trayResult);
    trayFinish.setSingleShot(true);
    connect(&trayFinish, &QTimer::timeout, this,
            [this]
            {
                trayResult->setText(lastTrayDetail);
                refreshTrayControls();
            });
    refreshTrayControls();
    body->addWidget(box);
}
bool selfTestDiceTray()
{
    auto original = character;
    bool oldDirty = dirty;
    auto fail = [&](QString message)
    {
        QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
        if (file.open(QIODevice::WriteOnly))
            file.write(("Dice tray: " + message).toUtf8());
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return false;
    };
    clearDiceTray();
    if (trayPreviews.size() != 7)
        return fail("Missing illustrated dice selectors");
    for (auto it = trayPreviews.begin(); it != trayPreviews.end(); ++it)
        if (it.value()->displayedFace() != it.key() || it.value()->rolling())
            return fail("Incorrect die preview");
    int before = array("rollLog").size();
    rollDiceTray();
    if (array("rollLog").size() != before || trayRollButton->isEnabled())
        return fail("empty tray rolled");
    for (auto *spin : trayCounts)
        spin->setValue(1);
    trayCounts[6]->setValue(2);
    trayModifier->setValue(-3);
    trayRollButton->click();
    if (!trayFinish.isActive() || trayRollButton->isEnabled() || trayDice.size() != 8 ||
        array("rollLog").size() != before + 1)
        return fail("mixed dice roll");
    rollDiceTray();
    clearDiceTray();
    if (array("rollLog").size() != before + 1 || trayDice.size() != 8)
        return fail("animation reentry");
    for (auto *visual : trayPreviews)
        if (!visual->rolling())
            return fail("Selector die did not animate");
    QEventLoop settle;
    QTimer::singleShot(1250, &settle, &QEventLoop::quit);
    settle.exec();
    int sum = -3;
    for (int i = 0; i < trayDice.size(); ++i)
    {
        auto entry = lastTrayDice[i].toObject();
        int sides = entry["sides"].toInt(), face = entry["face"].toInt();
        if (face < 1 || face > sides || trayDice[i]->rolling() ||
            trayDice[i]->displayedFace() != face)
            return fail("displayed die differs from journal result");
        sum += face;
    }
    if (sum != lastTrayTotal || !trayResult->text().contains(QString("TOTAL: %1").arg(sum)) ||
        !array("rollLog").last().toString().contains("modifier -3"))
        return fail("incorrect total");
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == "Dice journal")
            tabs->setCurrentIndex(i);
    QApplication::processEvents();
    grab().save(QCoreApplication::applicationDirPath() + "/preview-dice-tray.png");
    clearDiceTray();
    if (!trayDice.isEmpty() || trayRollButton->isEnabled() || array("rollLog").size() != before + 1)
        return fail("clear tray altered journal");
    trayCounts[6]->setValue(40);
    trayCounts[20]->stepUp();
    if (trayCounts[20]->value() != 0 || trayCounts[6]->value() != 40)
        return fail("dice count limit");
    clearDiceTray();
    character = original;
    loadWidgets();
    dirty = oldDirty;
    return true;
}

