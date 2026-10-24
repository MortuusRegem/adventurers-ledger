bool selfTestIncrementalChoices()
{
    auto original = character;
    bool oldDirty = dirty;
    auto finish = [&](QString error)
    {
        if (!error.isEmpty())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly))
                file.write(("Incremental level up: " + error).toUtf8());
        }
        character = original;
        loadWidgets();
        dirty = oldDirty;
        return error.isEmpty();
    };
    for (auto name : skillNames + abilities)
        if (basicChoiceDescription(name).size() < 80)
            return finish("Missing explanation: " + name);
    QString formatted = sourcesLast("Source: Book\nOlder subclass adapted to "
                                    "2024.\nhttps://example.com\n\nHow it works.\nRoll dice.");
    if (formatted.contains("Source:") || formatted.contains("Older subclass") ||
        formatted.contains("https://") || !formatted.contains("Roll dice") ||
        formatted != sourcesLast(formatted))
        return finish("Source details must be confined to Rules and help");
    auto fixture = [&](QString sub, int level)
    {
        character = blank();
        character["class"] = "Fighter";
        character["subclass"] = sub;
        character["primaryLevel"] = level;
        character["level"] = level;
        character["manageClassProgression"] = true;
    };
    fixture("Rune Knight", 7);
    QString key = subclassChoiceKey("Fighter", subclassDefinition("Fighter"), "Runes");
    character["classChoices"] = QJsonObject{{key, QJsonArray{"Cloud Rune", "Fire Rune"}}};
    int selections = 0, reviews = 0;
    bool bad = false, cancelReview = false;
    QTimer answer;
    connect(&answer, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                {
                    bad = true;
                    dialog->reject();
                    return;
                }
                bool review = dialog->windowTitle().startsWith("Unlocked subclass abilities");
                if (review && cancelReview)
                {
                    dialog->reject();
                    return;
                }
                int count = list->property("requiredChoiceCount").toInt();
                if (!review)
                {
                    ++selections;
                    bad |= count != 1;
                    for (int i = 0; i < list->count(); ++i)
                        bad |=
                            QStringList{"Cloud Rune", "Fire Rune"}.contains(list->item(i)->text());
                }
                else
                {
                    ++reviews;
                    for (int i = 0; i < list->count(); ++i)
                        bad |= !list->item(i)->text().startsWith("Level 7:");
                }
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setCheckState(i < count ? Qt::Checked : Qt::Unchecked);
                if (!review)
                    dialog->grab().save(QCoreApplication::applicationDirPath() +
                                        "/preview-additional-subclass-choice.png");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    answer.start(20);
    QStringList result;
    bool accepted = configureSubclassFeatures("Fighter", 7, result, true);
    answer.stop();
    auto picked = jsonStrings(character["classChoices"].toObject()[key].toArray());
    if (!accepted || bad || selections != 1 || reviews != 1 || picked.size() != 3 ||
        !picked.contains("Cloud Rune") || !picked.contains("Fire Rune"))
        return finish("Rune additions or previous selections");
    selections = reviews = 0;
    answer.start(20);
    accepted = configureSubclassFeatures("Fighter", 8, result, true);
    answer.stop();
    if (!accepted || selections || reviews)
        return finish("Unchanged choices reopened");
    auto before = character;
    character["classChoices"] = QJsonObject{{key, QJsonArray{"Cloud Rune", "Fire Rune"}}};
    before = character;
    cancelReview = true;
    answer.start(20);
    accepted = configureSubclassFeatures("Fighter", 7, result, true);
    answer.stop();
    if (accepted || character != before)
        return finish("Canceled addition changed saved choices");
    fixture("Echo Knight", 11);
    QStringList previous{"Club", "Dagger", "Greatclub", "Handaxe", "Javelin"};
    character["classMasteries"] = QJsonObject{{"Fighter", QJsonArray::fromStringList(previous)}};
    QTimer masteryAnswer;
    int masteryWindows = 0, classReviews = 0;
    bool descriptionChecked = false;
    connect(&masteryAnswer, &QTimer::timeout, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *list = dialog->findChild<QListWidget *>("levelUpChoices");
                if (!list)
                {
                    bad = true;
                    dialog->reject();
                    return;
                }
                bool mastery = dialog->windowTitle().contains("weapon masteries");
                int count = list->property("requiredChoiceCount").toInt();
                if (mastery)
                {
                    ++masteryWindows;
                    bad |= count != 1;
                    for (int i = 0; i < list->count(); ++i)
                        bad |= previous.contains(list->item(i)->text());
                    auto text = dialog->findChild<QPlainTextEdit *>()->toPlainText();
                    bad |= !text.contains("Mastery requires");
                }
                else
                {
                    ++classReviews;
                    auto text = dialog->findChild<QPlainTextEdit *>()->toPlainText();
                    descriptionChecked = !text.isEmpty() && !text.contains("Older subclass") &&
                                         !text.contains("Subclass: Echo Knight");
                    bad |= !descriptionChecked;
                    bad |= list->count() != 1;
                    dialog->grab().save(QCoreApplication::applicationDirPath() +
                                        "/preview-class-feature-explanation.png");
                }
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setCheckState(i < count ? Qt::Checked : Qt::Unchecked);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
    masteryAnswer.start(20);
    accepted = reviewLevelFeatures("Fighter", 11, "Review new class and subclass features", result);
    masteryAnswer.stop();
    if (!accepted || bad || masteryWindows || classReviews != 1 || !descriptionChecked)
        return finish("Unchanged weapon masteries or specific class explanation");
    character["level"] = 16;
    character["primaryLevel"] = 16;
    masteryAnswer.start(20);
    accepted = reviewLevelFeatures("Fighter", 16, "Review new class and subclass features", result);
    masteryAnswer.stop();
    picked = jsonStrings(character["classMasteries"].toObject()["Fighter"].toArray());
    if (!accepted || bad || masteryWindows != 1 || picked.size() != 6)
        return finish("Additional mastery allowance");
    for (auto name : previous)
        if (!picked.contains(name))
            return finish("Lost prior mastery");
    character = blank();
    character["class"] = "Fighter";
    character["primaryLevel"] = 3;
    character["level"] = 4;
    character["advancementHistory"] =
        QJsonArray{QJsonObject{{"class", "Fighter"}, {"classLevel", 3}, {"level", 4}}};
    character["advancementTasks"] = QJsonArray{};
    auto tasks = array("advancementTasks");
    addAdvanceTask(tasks, "Fighter", 2, "review", "Previous level");
    addAdvanceTask(tasks, "Rogue", 1, "review", "Previous multiclass level");
    addAdvanceTask(tasks, "Fighter", 3, "review", "Current level");
    character["advancementTasks"] = tasks;
    refreshAdvancement();
    if (advancementTasks->count() != 1 ||
        advancementTasks->item(0)->data(Qt::UserRole).toInt() != 2 ||
        array("advancementTasks").size() != 3)
        return finish("Current level action filter or saved task preservation");
    return finish({});
}
