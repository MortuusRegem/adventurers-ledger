#include <QtWidgets>
#include <QtNetwork>
#include "updater.h"
#include "theme.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

const QStringList abilities = {"Strength",     "Dexterity", "Constitution",
                               "Intelligence", "Wisdom",    "Charisma"};
const QStringList classes = {"Artificer", "Barbarian", "Bard",    "Cleric", "Druid",
                             "Fighter",   "Monk",      "Paladin", "Ranger", "Rogue",
                             "Sorcerer",  "Warlock",   "Wizard"};
// Core 2024 subclasses; supplemental book entries are merged before building the UI.
QMap<QString, QStringList> subclasses = {
    {"Barbarian",
     {"Path of the Berserker", "Path of the Wild Heart", "Path of the World Tree",
      "Path of the Zealot"}},
    {"Bard", {"College of Dance", "College of Glamour", "College of Lore", "College of Valor"}},
    {"Cleric", {"Life Domain", "Light Domain", "Trickery Domain", "War Domain"}},
    {"Druid",
     {"Circle of the Land", "Circle of the Moon", "Circle of the Sea", "Circle of the Stars"}},
    {"Fighter", {"Battle Master", "Champion", "Eldritch Knight", "Psi Warrior"}},
    {"Monk",
     {"Warrior of Mercy", "Warrior of Shadow", "Warrior of the Elements",
      "Warrior of the Open Hand"}},
    {"Paladin", {"Oath of Devotion", "Oath of Glory", "Oath of the Ancients", "Oath of Vengeance"}},
    {"Ranger", {"Beast Master", "Fey Wanderer", "Gloom Stalker", "Hunter"}},
    {"Rogue", {"Arcane Trickster", "Assassin", "Soulknife", "Thief"}},
    {"Sorcerer",
     {"Aberrant Sorcery", "Clockwork Sorcery", "Draconic Sorcery", "Wild Magic Sorcery"}},
    {"Warlock", {"Archfey Patron", "Celestial Patron", "Fiend Patron", "Great Old One Patron"}},
    {"Wizard", {"Abjurer", "Diviner", "Evoker", "Illusionist"}}};
QJsonArray supplementalSubclassCatalog()
{
    static const QJsonArray entries = []
    {
        QFile file(":/subclasses.json");
        return file.open(QIODevice::ReadOnly)
                   ? QJsonDocument::fromJson(file.readAll()).object()["subclasses"].toArray()
                   : QJsonArray{};
    }();
    return entries;
}
QJsonArray subclassFeatureCatalog()
{
    static const QJsonArray entries = []
    {
        QFile file(":/subclass_features.json");
        return file.open(QIODevice::ReadOnly)
                   ? QJsonDocument::fromJson(file.readAll()).object()["subclasses"].toArray()
                   : QJsonArray{};
    }();
    return entries;
}
QJsonObject artificerReference()
{
    static const QJsonObject reference = []
    {
        QFile file(":/artificer.json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                              : QJsonObject{};
    }();
    return reference;
}
#include "description_text.inc"
#include "subclass_info.inc"
void populateSubclasses(QComboBox *combo, const QString &cls, const QString &selected = {},
                        bool preserveExisting = true)
{
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem("");
    combo->addItems(subclasses.value(cls));
    if (preserveExisting && !selected.isEmpty() && combo->findText(selected) < 0)
        combo->addItem(selected);
    combo->setCurrentIndex(qMax(0, combo->findText(selected)));
    auto descriptions = subclassDescriptions();
    for (int i = 0; i < combo->count(); ++i)
        combo->setItemData(i, descriptions.value(combo->itemText(i)), Qt::ToolTipRole);
    combo->setToolTip("Choose a subclass for this class. Subclasses begin at class level 3; leave "
                      "blank if not chosen yet.");
}
const QStringList skillNames = {
    "Acrobatics",  "Animal Handling", "Arcana",        "Athletics",       "Deception", "History",
    "Insight",     "Intimidation",    "Investigation", "Medicine",        "Nature",    "Perception",
    "Performance", "Persuasion",      "Religion",      "Sleight of Hand", "Stealth",   "Survival"};
const QList<int> skillAbilities = {1, 4, 3, 0, 5, 3, 4, 5, 3, 4, 3, 4, 5, 5, 3, 1, 1, 4};
int modifier(int score)
{
    return int(std::floor((score - 10) / 2.0));
}
int proficiency(int level)
{
    return 2 + (qBound(1, level, 20) - 1) / 4;
}
QString sign(int n)
{
    return (n >= 0 ? "+" : "") + QString::number(n);
}
int die(int sides)
{
    return QRandomGenerator::system()->bounded(sides) + 1;
}
QIcon ledgerIcon()
{
    QPixmap image(256, 256);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPolygonF shape;
    shape << QPointF(128, 10) << QPointF(235, 72) << QPointF(235, 184) << QPointF(128, 246)
          << QPointF(21, 184) << QPointF(21, 72);
    painter.setPen(QPen(QColor("#c9a962"), 7));
    painter.setBrush(QColor("#61352f"));
    painter.drawPolygon(shape);
    painter.setPen(QPen(QColor("#c9a962"), 3));
    painter.drawLine(128, 10, 68, 86);
    painter.drawLine(128, 10, 188, 86);
    painter.drawLine(21, 72, 68, 86);
    painter.drawLine(235, 72, 188, 86);
    painter.drawLine(68, 86, 188, 86);
    painter.drawLine(68, 86, 40, 185);
    painter.drawLine(188, 86, 216, 185);
    painter.drawLine(40, 185, 216, 185);
    painter.drawLine(40, 185, 128, 246);
    painter.drawLine(216, 185, 128, 246);
    painter.setPen(QColor("#f5e3b6"));
    painter.setFont(QFont("Georgia", 53, QFont::Bold));
    painter.drawText(QRect(38, 87, 180, 97), Qt::AlignCenter, "20");
    painter.end();
    return QIcon(image);
}
struct Roll
{
    int total = 0;
    QString detail;
    int face = 0, sides = 20;
};

class RollingDie : public QWidget
{
    QTimer timer;
    QElapsedTimer elapsed;
    Roll result;
    int face = 20;
    bool hasResult = false;
    int previewSides = 20;

  public:
    explicit RollingDie(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(96, 88);
        setAccessibleName("Animated dice result");
        timer.setInterval(35);
        connect(&timer, &QTimer::timeout, this,
                [this]
                {
                    if (elapsed.elapsed() >= 950)
                    {
                        timer.stop();
                        face = result.face;
                        setAccessibleDescription(
                            QString("Result: %1. %2").arg(result.total).arg(result.detail));
                    }
                    else
                        face = die(qBound(2, result.sides, 1000));
                    update();
                });
    }
    void preview(int sides)
    {
        timer.stop();
        previewSides = sides;
        face = sides;
        hasResult = false;
        setToolTip("d" + QString::number(sides));
        update();
    }
    void animate(const Roll &roll)
    {
        result = roll;
        hasResult = true;
        elapsed.restart();
        timer.start();
        setToolTip(roll.detail + " = " + QString::number(roll.total));
        update();
    }
    bool rolling() const
    {
        return timer.isActive();
    }
    int displayedFace() const
    {
        return face;
    }
    int displayedTotal() const
    {
        return result.total;
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool moving = timer.isActive();
        double progress = moving ? qMin(1.0, elapsed.elapsed() / 950.0) : 1.0;
        double bounce = moving ? std::abs(std::sin(progress * 16.0)) * (1.0 - progress) * 7 : 0;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(65, 40, 25, 35));
        p.drawEllipse(QRectF(22 + bounce, 58, 52 - 2 * bounce, 8));
        p.save();
        p.translate(48, 34 - bounce);
        if (moving)
            p.rotate(720.0 * (1.0 - std::pow(1.0 - progress, 3)));
        p.setPen(QPen(QColor("#c5a15b"), 1.8));
        p.setBrush(QColor("#693c33"));
        int sides = hasResult ? result.sides : previewSides;
        if (sides == 6)
        {
            p.drawRoundedRect(QRectF(-25, -25, 50, 50), 9, 9);
        }
        else
        {
            int vertices = sides == 4 ? 3 : sides == 8 || sides == 10 ? 4 : sides == 12 ? 5 : 6;
            QPolygonF shape;
            for (int i = 0; i < vertices; ++i)
            {
                double angle = -1.57079632679 + i * 6.28318530718 / vertices;
                shape << QPointF(29 * std::cos(angle), 29 * std::sin(angle));
            }
            p.drawPolygon(shape);
            p.setPen(QPen(QColor("#a77e52"), 1));
            for (int i = 0; i < vertices; ++i)
            {
                QPointF inner = shape[i] * 0.65;
                p.drawLine(shape[i], inner);
                p.drawLine(inner, shape[(i + 1) % vertices] * 0.65);
            }
        }
        p.setPen(QColor("#fff0c8"));
        p.setFont(QFont("Georgia", face > 99 ? 13 : 19, QFont::Bold));
        p.drawText(QRectF(-23, -20, 46, 40), Qt::AlignCenter,
                   face ? QString::number(face) : QString("—"));
        p.restore();
        p.setPen(palette().color(QPalette::WindowText));
        p.setFont(QFont("Palatino Linotype", 10, QFont::Bold));
        p.drawText(QRect(0, 66, width(), 22), Qt::AlignCenter,
                   moving      ? "Rolling…"
                   : hasResult ? QString("Total %1").arg(result.total)
                               : "d" + QString::number(previewSides));
    }
};
Roll rollDice(QString expression, bool critical = false)
{
    expression.remove(' ');
    expression = expression.toLower();
    static const QRegularExpression full("^(?:[+-]?(?:[0-9]{1,3}d[0-9]{1,4}|[0-9]{1,6}))+$");
    if (!full.match(expression).hasMatch())
        throw std::runtime_error("Use dice such as 1d20+5, 2d6+3, or 1d8+1d6+2.");
    Roll result;
    QStringList parts;
    auto tokens = QRegularExpression("([+-]?)([0-9]+)(?:d([0-9]+))?").globalMatch(expression);
    int rolled = 0;
    while (tokens.hasNext())
    {
        auto m = tokens.next();
        int direction = m.captured(1) == "-" ? -1 : 1;
        int count = m.captured(2).toInt();
        if (m.captured(3).isEmpty())
        {
            result.total += direction * count;
            parts << ((direction < 0 ? "-" : "+") + QString::number(count));
            continue;
        }
        int sides = m.captured(3).toInt();
        if (critical)
            count *= 2;
        if (sides < 2 || sides > 1000 || count < 1 || count > 200 || rolled + count > 300)
            throw std::runtime_error(
                "Use 1–200 dice per term, 2–1000 sides, and at most 300 dice in a roll.");
        rolled += count;
        QStringList values;
        int subtotal = 0;
        for (int i = 0; i < count; ++i)
        {
            int value = die(sides);
            result.face = value;
            result.sides = sides;
            subtotal += value;
            values << QString::number(value);
        }
        result.total += direction * subtotal;
        parts << QString("%1%2d%3 [%4]")
                     .arg(direction < 0 ? "-" : "+")
                     .arg(count)
                     .arg(sides)
                     .arg(values.join(", "));
    }
    result.detail = parts.join(" ");
    if (result.detail.startsWith('+'))
        result.detail.remove(0, 1);
    return result;
}

class Ledger : public QMainWindow
{
    QJsonObject character, catalog, advancementCatalog;
    QWidget *subclassFeaturePage = nullptr;
    QTableWidget *subclassFeatureTable = nullptr;
    QLineEdit *subclassFeatureSearch = nullptr;
    QCheckBox *showFutureSubclassFeatures = nullptr;
    QPlainTextEdit *subclassFeatureDetails = nullptr;
    QPushButton *featureChooseButton = nullptr, *featureUseButton = nullptr,
                *featureRollButton = nullptr, *featureToggleButton = nullptr,
                *featureCompanionButton = nullptr, *featureRestoreButton = nullptr;
    QWidget *advancementPage = nullptr;
    QLabel *advancementStatus = nullptr;
    QListWidget *advancementTasks = nullptr;
    QPlainTextEdit *advancementDetails = nullptr;
    QTableWidget *classResourceTable = nullptr;
    QCheckBox *classTraitsCheck = nullptr;
    QJsonArray featCatalog, speciesCatalog, equipmentCatalog, backgroundCatalog;
    QLabel *abilityLockState = nullptr;
    QLabel *abilityBudgetStatus = nullptr;
    QLabel *creationStatus = nullptr;
    QLabel *deityDetails = nullptr;
    QProgressBar *carryingProgress = nullptr;
    QList<QPushButton *> abilitySetupButtons;
    QWidget *featPage = nullptr;
    QTableWidget *featTable = nullptr, *sourceTable = nullptr;
    QLabel *effectiveStats = nullptr;
    QCheckBox *armorCheck = nullptr;
    QCheckBox *legacyPdfSpellsCheck = nullptr;
    QCheckBox *attackActionCheck = nullptr;
    QPlainTextEdit *effectsOverview = nullptr;
    QString directory, current;
    bool loading = false, dirty = false;
    QTimer autosave;
    QMap<QString, QWidget *> fields;
    QListWidget *characters;
    QLineEdit *search;
    QLabel *summary, *saveState, *profLabel, *initiativeLabel, *passiveLabel, *spellLabel, *hpState,
        *weightLabel;
    QLabel *modLabels[6], *saveLabels[6], *skillLabels[18];
    QCheckBox *saveProf[6];
    QComboBox *skillProf[18];
    QTableWidget *weaponTable, *spellTable, *inventoryTable, *slotTable;
    QPlainTextEdit *spellDetails, *rollHistory;
    QComboBox *rollMode;
    QComboBox *diceInput;
    RollingDie *diceVisual;
    QMap<int, QSpinBox *> trayCounts;
    QMap<int, RollingDie *> trayPreviews;
    QSpinBox *trayModifier = nullptr;
    QPushButton *trayRollButton = nullptr, *trayClearButton = nullptr;
    QLabel *trayCounter = nullptr, *trayResult = nullptr;
    QGridLayout *trayGrid = nullptr;
    QList<RollingDie *> trayDice;
    QTimer trayFinish;
    QJsonArray lastTrayDice;
    int lastTrayTotal = 0;
    QString lastTrayDetail;
    QCheckBox *criticalDamage;
    QTabWidget *tabs;
    int number(const QString &key, int fallback = 0) const
    {
        return character.value(key).toInt(fallback);
    }
    QString text(const QString &key) const
    {
        return character.value(key).toString();
    }
    int ability(int index) const
    {
        return modifier(effectiveScore(index));
    }
    int pb() const
    {
        return proficiency(number("level", 1));
    }
    QJsonArray array(const QString &key) const
    {
        return character.value(key).toArray();
    }
#include "ability_budget.inc"
#include "advancement.inc"
#include "book_catalogs.inc"
#include "creation.inc"
#include "deities.inc"
#include "dice_tray.inc"
#include "effects.inc"
#include "encumbrance.inc"
#include "equipment.inc"
#include "level_choices.inc"
#include "levelup_increment_tests.inc"
#include "progression.inc"
#include "subclass_features.inc"
#include "subclass_rules.inc"
#include "uploaded_rules.inc"
    void put(const QString &key, const QJsonValue &value)
    {
        if (!loading && abilities.contains(key) &&
            (baseAbilitiesProtected() || !allowedBaseScore(key, value.toInt())))
        {
            if (auto *w = qobject_cast<QSpinBox *>(fields.value(key)))
            {
                QSignalBlocker blocker(w);
                w->setValue(number(key));
            }
            return;
        }
        character[key] = value;
        if (!loading && (key == "subclass" || key == "multiclasses" || key == "primaryLevel"))
        {
            if (key == "subclass" && !subclassDefinition(text("class")).isEmpty() &&
                classLevel(text("class")) >= 3)
                character["manageClassProgression"] = true;
            syncSubclassSpells();
            refreshSpells();
        }
        if (!loading && !creationProblem().isEmpty())
            character["abilitiesLocked"] = false;
        if (!loading)
        {
            dirty = true;
            saveState->setText("Saving…");
            autosave.start(600);
            refreshDerived();
        }
    }
    QSpinBox *spin(const QString &key, int minimum = 0, int maximum = 9999)
    {
        auto *w = new QSpinBox;
        w->setRange(minimum, maximum);
        fields[key] = w;
        connect(w, &QSpinBox::valueChanged, this, [this, key](int n) { put(key, n); });
        return w;
    }
    QLineEdit *line(const QString &key)
    {
        auto *w = new QLineEdit;
        fields[key] = w;
        connect(w, &QLineEdit::textChanged, this, [this, key](const QString &s) { put(key, s); });
        return w;
    }
    QComboBox *choice(const QString &key, const QStringList &items, bool editable = false)
    {
        auto *w = new QComboBox;
        w->addItems(items);
        w->setEditable(editable);
        fields[key] = w;
        connect(w, &QComboBox::currentTextChanged, this,
                [this, key](const QString &s) { put(key, s); });
        return w;
    }
    QPlainTextEdit *notes(const QString &key)
    {
        auto *w = new QPlainTextEdit;
        fields[key] = w;
        connect(w, &QPlainTextEdit::textChanged, this,
                [this, key, w] { put(key, w->toPlainText()); });
        return w;
    }
    QPushButton *button(const QString &label, std::function<void()> callback)
    {
        auto *w = new QPushButton(label);
        connect(w, &QPushButton::clicked, this, std::move(callback));
        return w;
    }
    QWidget *page(QVBoxLayout *&layout)
    {
        auto *body = new QWidget;
        layout = new QVBoxLayout(body);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(16);
        return body;
    }
    QTableWidget *table(const QStringList &headers, bool editable = false)
    {
        auto *w = new QTableWidget(0, headers.size());
        w->setHorizontalHeaderLabels(headers);
        w->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        w->horizontalHeader()->setStretchLastSection(true);
        w->verticalHeader()->hide();
        w->setSelectionBehavior(QAbstractItemView::SelectRows);
        w->setSelectionMode(QAbstractItemView::SingleSelection);
        w->setAlternatingRowColors(true);
        if (!editable)
            w->setEditTriggers(QAbstractItemView::NoEditTriggers);
        return w;
    }
    QWidget *scroll(QWidget *body)
    {
        auto *s = new QScrollArea;
        s->setWidgetResizable(true);
        s->setWidget(body);
        s->setFrameShape(QFrame::NoFrame);
        return s;
    }
    void displayError(const QString &message)
    {
        QMessageBox::warning(this, "Adventurer’s Ledger", message);
    }
    QString path() const
    {
        return directory + "/" + current + ".json";
    }
    bool save()
    {
        if (!dirty || current.isEmpty())
            return true;
        character["schema"] = 1;
        character["updated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        QSaveFile f(path());
        auto bytes = QJsonDocument(character).toJson();
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        {
            saveState->setText("Save failed — export a backup");
            return false;
        }
        dirty = false;
        saveState->setText("Saved locally");
        for (int i = 0; i < characters->count(); ++i)
            if (characters->item(i)->data(Qt::UserRole).toString() == current)
                characters->item(i)->setText(text("name").isEmpty() ? "Unnamed adventurer"
                                                                    : text("name"));
        return true;
    }
    QJsonObject blank()
    {
        QJsonObject c{{"schema", 1},
                      {"abilitiesLocked", false},
                      {"creationRequired", false},
                      {"abilityBudgetManaged", false},
                      {"name", "New adventurer"},
                      {"species", "Human"},
                      {"class", "Fighter"},
                      {"background", "Soldier"},
                      {"gender", ""},
                      {"deity", "None"},
                      {"faithNotes", ""},
                      {"carryingSize", "Small / Medium"},
                      {"carryingTrait", "Automatic"},
                      {"includeCoinWeight", true},
                      {"languages", "Common"},
                      {"level", 1},
                      {"hpMax", 11},
                      {"hpCurrent", 11},
                      {"ac", 12},
                      {"speed", 30},
                      {"spellAbility", "Intelligence"},
                      {"Strength", 15},
                      {"Dexterity", 14},
                      {"Constitution", 13},
                      {"Intelligence", 12},
                      {"Wisdom", 10},
                      {"Charisma", 8}};
        c["saveProfs"] = QJsonArray{true, false, true, false, false, false};
        return c;
    }
    void refreshLibrary()
    {
        QSignalBlocker b(characters);
        characters->clear();
        for (const QString &name : QDir(directory).entryList({"*.json"}, QDir::Files, QDir::Time))
        {
            QFile f(directory + "/" + name);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            auto doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isObject())
                continue;
            auto c = doc.object();
            if (!(c["name"].toString() + " " + c["class"].toString() + " " +
                  c["species"].toString())
                     .contains(search->text(), Qt::CaseInsensitive))
                continue;
            auto *item = new QListWidgetItem(c["name"].toString().isEmpty() ? "Unnamed adventurer"
                                                                            : c["name"].toString(),
                                             characters);
            item->setData(Qt::UserRole, QFileInfo(name).completeBaseName());
            item->setToolTip(c["species"].toString() + " · " + c["class"].toString());
            if (item->data(Qt::UserRole).toString() == current)
                characters->setCurrentItem(item);
        }
    }
    void setNumber(const QString &key, int value)
    {
        if (auto *s = qobject_cast<QSpinBox *>(fields.value(key)))
            s->setValue(value);
    }
    void loadWidgets()
    {
        if (!character.contains("creationRequired"))
            character["creationRequired"] =
                number("level", 1) == 1 && !character["abilitiesLocked"].toBool(true);
        if (!creationProblem().isEmpty())
            character["abilitiesLocked"] = false;
        if (!character.contains("abilityBudgetManaged") && character["creationRequired"].toBool() &&
            !character["abilitiesLocked"].toBool() && number("level", 1) == 1)
        {
            QJsonArray pool;
            auto bg = character["backgroundIncreases"].toObject();
            for (auto a : abilities)
                pool.append(number(a) - bg[a].toInt());
            character["abilityPool"] = pool;
            character["abilityBudgetManaged"] = true;
            character["abilityMethod"] = "Previously assigned scores";
        }
        if (!character.contains("abilitiesLocked"))
            character["abilitiesLocked"] = true;
        if (!character.contains("armorDexMode"))
            character["armorDexMode"] = "Full";
        if (!character.contains("primaryLevel"))
            character["primaryLevel"] = number("level", 1);
        if (text("class") == "Warlock" && !character.contains("pactMax") &&
            array("multiclasses").isEmpty())
        {
            int n = number("level", 1), index = qMin(5, (n + 1) / 2) - 1;
            character["pactLevel"] = index + 1;
            character["pactMax"] = array("slotsMax").at(index).toInt();
            character["pactUsed"] = array("slotsUsed").at(index).toInt();
            character["slotsMax"] = QJsonArray{};
            character["slotsUsed"] = QJsonArray{};
        }
        syncSubclassSpells();
        if (!character.contains("carryingSize"))
            character["carryingSize"] = "Small / Medium";
        if (!character.contains("carryingTrait"))
            character["carryingTrait"] = "Automatic";
        if (!character.contains("includeCoinWeight"))
            character["includeCoinWeight"] = true;
        loading = true;
        for (auto a : abilities)
            if (auto *spin = qobject_cast<QSpinBox *>(fields.value(a)))
            {
                QSignalBlocker block(spin);
                spin->setRange(1, 30);
            }
        populateSubclasses(qobject_cast<QComboBox *>(fields["subclass"]), text("class"),
                           text("subclass"));
        qobject_cast<QSpinBox *>(fields["hpCurrent"])->setMaximum(9999);
        for (auto it = fields.begin(); it != fields.end(); ++it)
        {
            const auto v = character[it.key()];
            auto *w = it.value();
            if (auto *s = qobject_cast<QSpinBox *>(w))
            {
                s->setValue(v.toInt());
                character[it.key()] = s->value();
            }
            else if (auto *l = qobject_cast<QLineEdit *>(w))
                l->setText(v.toString());
            else if (auto *c = qobject_cast<QComboBox *>(w))
            {
                QString value = v.toString();
                if (c->findText(value) < 0 && !c->isEditable())
                    c->addItem(value);
                c->setCurrentText(value);
            }
            else if (auto *check = qobject_cast<QCheckBox *>(w))
                check->setChecked(v.toBool());
            else if (auto *p = qobject_cast<QPlainTextEdit *>(w))
                p->setPlainText(v.toString());
        }
        const auto saves = array("saveProfs"), skills = array("skillProfs");
        QJsonArray normalizedSaves, normalizedSkills;
        for (int i = 0; i < 6; ++i)
        {
            saveProf[i]->setChecked(saves.at(i).toBool());
            normalizedSaves.append(saveProf[i]->isChecked());
        }
        for (int i = 0; i < 18; ++i)
        {
            skillProf[i]->setCurrentIndex(qBound(0, skills.at(i).toInt(), 2));
            normalizedSkills.append(skillProf[i]->currentIndex());
        }
        refreshDeityDescription();
        character["saveProfs"] = normalizedSaves;
        character["skillProfs"] = normalizedSkills;
        if (legacyPdfSpellsCheck)
        {
            QSignalBlocker blocker(legacyPdfSpellsCheck);
            legacyPdfSpellsCheck->setChecked(character["allowLegacyPdfRules"].toBool());
        }
        if (armorCheck)
        {
            QSignalBlocker b(armorCheck);
            armorCheck->setChecked(character["wearingArmor"].toBool());
        }
        if (attackActionCheck)
        {
            QSignalBlocker b(attackActionCheck);
            attackActionCheck->setChecked(character["attackAction"].toBool(true));
        }
        populateInventory();
        populateSlots();
        refreshWeapons();
        refreshSpells();
        refreshFeats();
        rollHistory->clear();
        for (const auto &entry : array("rollLog"))
            rollHistory->appendPlainText(entry.toString());
        if (classTraitsCheck)
        {
            QSignalBlocker blocker(classTraitsCheck);
            classTraitsCheck->setChecked(character["classTraitsActive"].toBool(true));
        }
        loading = false;
        dirty = false;
        autosave.stop();
        refreshDerived();
        saveState->setText("Saved locally");
    }
    void openCharacter(const QString &id)
    {
        if (id == current)
            return;
        if (!save())
        {
            displayError(
                "Your current character could not be saved. Export a backup before switching.");
            refreshLibrary();
            return;
        }
        QFile f(directory + "/" + id + ".json");
        if (!f.open(QIODevice::ReadOnly))
        {
            displayError("Could not open that character.");
            return;
        }
        QJsonParseError e;
        auto doc = QJsonDocument::fromJson(f.readAll(), &e);
        if (e.error != QJsonParseError::NoError || !doc.isObject() ||
            doc.object()["schema"].toInt() != 1)
        {
            displayError("This character file is damaged or uses an unsupported format. It has "
                         "been preserved.");
            refreshLibrary();
            return;
        }
        current = id;
        character = doc.object();
        loadWidgets();
    }
    void newCharacter()
    {
        if (!save())
        {
            displayError("Save or export your current character first.");
            return;
        }
        current = QUuid::createUuid().toString(QUuid::WithoutBraces);
        character = blank();
        character["creationRequired"] = true;
        character["abilityBudgetManaged"] = true;
        character["abilityPool"] = QJsonArray{15, 14, 13, 12, 10, 8};
        character["abilityMethod"] = "Standard array";
        loadWidgets();
        dirty = true;
        if (save())
            refreshLibrary();
        tabs->setCurrentIndex(0);
        fields["name"]->setFocus();
    }
    void log(const QString &label, const QString &details)
    {
        QString entry = QTime::currentTime().toString("HH:mm:ss") + "  " + label + "\n" + details;
        rollHistory->appendPlainText(entry + "\n");
        auto history = array("rollLog");
        history.append(entry);
        while (history.size() > 200)
            history.removeFirst();
        put("rollLog", history);
        statusBar()->showMessage(label + " — " + details, 12000);
    }
    Roll d20(const QString &label, int bonus, const QStringList &targets = {})
    {
        if (label == "Initiative")
            subclassInitiativeTriggers();
        int a = die(20), b = die(20), used = a;
        QString detail = QString("d20 [%1]").arg(a);
        bool advantage = rollMode->currentIndex() == 1,
             disadvantage = rollMode->currentIndex() == 2;
        for (auto v : activeEffects())
        {
            auto e = v.toObject();
            if (!targets.contains(e["target"].toString()))
                continue;
            advantage |= e["op"] == "advantage";
            disadvantage |= e["op"] == "disadvantage";
        }
        if (advantage && !disadvantage)
        {
            used = qMax(a, b);
            detail = QString("advantage [%1, %2], keep %3").arg(a).arg(b).arg(used);
        }
        if (disadvantage && !advantage)
        {
            used = qMin(a, b);
            detail = QString("disadvantage [%1, %2], keep %3").arg(a).arg(b).arg(used);
        }
        if (character["manageClassProgression"].toBool() && classLevel("Rogue") >= 7 && used < 10)
        {
            for (int i = 0; i < skillNames.size(); ++i)
                if (targets.contains("skill:" + skillNames[i]) &&
                    effectTraining({"skill:*", "skill:" + skillNames[i]},
                                   array("skillProfs").at(i).toInt()) > 0)
                {
                    detail += "; Reliable Talent treats the d20 as 10";
                    used = 10;
                    break;
                }
        }
        QString extra = effectDice(targets);
        int extraTotal = 0;
        if (!extra.isEmpty())
        {
            auto r = rollDice(extra);
            extraTotal = r.total;
            detail += "; " + r.detail;
        }
        log(label, QString("%1 %2 = %3%4")
                       .arg(detail, sign(bonus))
                       .arg(used + bonus + extraTotal)
                       .arg(used == 20  ? "  (natural 20)"
                            : used == 1 ? "  (natural 1)"
                                        : ""));
        return Roll{used + bonus + extraTotal, detail + " " + sign(bonus), used, 20};
    }
    int skillBonus(int index) const
    {
        return ability(skillAbilities[index]) +
               effectTraining({"skill:*", "skill:" + skillNames[index]},
                              array("skillProfs").at(index).toInt()) *
                   pb() +
               effectBonus(skillTargets(index)) + classJackBonus(index);
    }
    void refreshDerived()
    {
        if (loading)
            return;
        updateAbilityLock();
        refreshAbilityBudget();
        refreshCreation();
        qobject_cast<QSpinBox *>(fields["hpCurrent"])->setMaximum(maximumHp());
        profLabel->setText(sign(pb()));
        initiativeLabel->setText(sign(initiativeBonus()) +
                                 effectDice({"initiative", "check:*", "check:Dexterity"}));
        passiveLabel->setText(QString::number(10 + skillBonus(11) +
                                              effectBonus({"passivePerception"}) +
                                              5 * effectAdvantage(skillTargets(11))));
        int casting = abilities.indexOf(text("spellAbility"));
        if (casting < 0)
            casting = 3;
        spellLabel->setText(QString("Attack %1   ·   Save DC %2")
                                .arg(sign(ability(casting) + pb() + effectBonus({"spellAttack"})) +
                                     effectDice({"spellAttack"}))
                                .arg(8 + pb() + ability(casting) + effectBonus({"spellDC"})));
        for (int i = 0; i < 6; ++i)
        {
            modLabels[i]->setText(QString("%1 (%2)").arg(effectiveScore(i)).arg(sign(ability(i))));
            modLabels[i]->setToolTip("Ability check: " + sign(checkBonus(i)) +
                                     effectDice({"check:*", "check:" + abilities[i]}));
            saveLabels[i]->setText(sign(saveBonus(i)) + effectDice(saveTargets(i)));
        }
        for (int i = 0; i < 18; ++i)
        {
            const int manual = array("skillProfs").at(i).toInt();
            const int training = effectTraining({"skill:*", "skill:" + skillNames[i]}, manual);
            QSignalBlocker blocker(skillProf[i]);
            skillProf[i]->setCurrentIndex(training);
            skillProf[i]->setToolTip(
                "Shows total training, including class, background, species, feats and items. "
                "Choose Proficient for one proficiency bonus, or Expertise for twice that bonus. "
                "None removes manual training; granted training remains until its source is "
                "changed.");
            skillLabels[i]->setText(sign(skillBonus(i)) + effectDice(skillTargets(i)));
        }
        summary->setText(QString("%1\nLevel %2 %3 · %4")
                             .arg(text("name").isEmpty() ? "Unnamed adventurer" : text("name"))
                             .arg(number("level", 1))
                             .arg(classSummary(), text("species")));
        hpState->setText(number("hpCurrent") > 0       ? "Conscious"
                         : number("deathFails") >= 3   ? "Three failed death saves"
                         : number("deathSuccess") >= 3 ? "Stable at 0 HP"
                                                       : "At 0 HP — death saves");
        refreshWeapons();
        updateWeight();
        updateEffectSummary();
        refreshAdvancement();
        refreshSubclassFeatureUi();
    }
    int hitDie(QString c = {}) const
    {
        if (c.isEmpty())
            c = text("class");
        if (c == "Barbarian")
            return 12;
        if (c == "Fighter" || c == "Paladin" || c == "Ranger")
            return 10;
        if (c == "Sorcerer" || c == "Wizard")
            return 6;
        return 8;
    }
    QJsonArray classRoster() const
    {
        QJsonArray result{QJsonObject{{"name", text("class")},
                                      {"level", number("primaryLevel", number("level", 1))},
                                      {"subclass", text("subclass")}}};
        for (auto v : array("multiclasses"))
            result.append(v);
        return result;
    }
    QString classSummary() const
    {
        QStringList names;
        for (auto v : classRoster())
        {
            auto c = v.toObject();
            names << c["name"].toString() + " " + QString::number(c["level"].toInt());
        }
        return names.join(" / ");
    }
    int averageHp() const
    {
        int hp = 0;
        int baseCon = modifier(number("Constitution", 10));
        bool first = true;
        for (auto v : classRoster())
        {
            auto c = v.toObject();
            int d = hitDie(c["name"].toString());
            int n = c["level"].toInt();
            hp += n * qMax(1, d / 2 + 1 + baseCon);
            if (first)
            {
                hp += qMax(1, d + baseCon) - qMax(1, d / 2 + 1 + baseCon);
                first = false;
            }
        }
        return hp;
    }
    void editClasses()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("Classes and levels");
        dialog.resize(880, 440);
        QVBoxLayout layout(&dialog);
        QLabel note(
            "The first row is your starting class. Total level cannot exceed 20. "
            "Added classes do not grant starting-class saving throws. Check ability prerequisites "
            "and class features in the rules reference. Choose each subclass at class level 3, "
            "or leave it blank until then.");
        note.setWordWrap(true);
        layout.addWidget(&note);
        QTableWidget roster(0, 4);
        roster.setHorizontalHeaderLabels({"Class", "Level", "Subclass", "Hit Dice"});
        roster.horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        roster.setColumnWidth(0, 160);
        roster.setColumnWidth(1, 80);
        roster.setColumnWidth(2, 390);
        roster.horizontalHeader()->setStretchLastSection(true);
        layout.addWidget(&roster);
        auto add = [&](QString name, int level, QString subclass)
        {
            int r = roster.rowCount();
            roster.insertRow(r);
            auto *c = new QComboBox;
            c->addItems(classes);
            c->setCurrentText(name);
            roster.setCellWidget(r, 0, c);
            auto *n = new QSpinBox;
            n->setRange(1, 20);
            n->setValue(level);
            roster.setCellWidget(r, 1, n);
            auto *sub = new QComboBox;
            populateSubclasses(sub, name, subclass);
            roster.setCellWidget(r, 2, sub);
            connect(c, &QComboBox::currentTextChanged, &dialog, [sub](const QString &cls)
                    { populateSubclasses(sub, cls, sub->currentText(), false); });
            auto *dice = new QLabel;
            roster.setCellWidget(r, 3, dice);
            auto update = [this, c, n, dice]
            { dice->setText(QString("%1d%2").arg(n->value()).arg(hitDie(c->currentText()))); };
            connect(c, &QComboBox::currentTextChanged, &dialog, update);
            connect(n, &QSpinBox::valueChanged, &dialog, update);
            update();
        };
        for (auto v : classRoster())
        {
            auto c = v.toObject();
            add(c["name"].toString(), c["level"].toInt(), c["subclass"].toString());
        }
        QHBoxLayout controls;
        layout.addLayout(&controls);
        auto *plus = new QPushButton("Add class");
        controls.addWidget(plus);
        connect(plus, &QPushButton::clicked, &dialog, [&] { add("Wizard", 1, {}); });
        auto *minus = new QPushButton("Remove selected class");
        controls.addWidget(minus);
        connect(minus, &QPushButton::clicked, &dialog,
                [&]
                {
                    int r = roster.currentRow();
                    if (r > 0)
                        roster.removeRow(r);
                });
        QDialogButtonBox actions(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        layout.addWidget(&actions);
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(
            &actions, &QDialogButtonBox::accepted, &dialog,
            [&]
            {
                QSet<QString> names;
                QJsonArray extra;
                int total = 0;
                QString primary, sub;
                int primaryLevel = 1;
                for (int r = 0; r < roster.rowCount(); ++r)
                {
                    QString name =
                        qobject_cast<QComboBox *>(roster.cellWidget(r, 0))->currentText();
                    int n = qobject_cast<QSpinBox *>(roster.cellWidget(r, 1))->value();
                    QString sc = qobject_cast<QComboBox *>(roster.cellWidget(r, 2))->currentText();
                    if (names.contains(name))
                    {
                        QMessageBox::warning(&dialog, "Classes",
                                             "Each class can appear only once.");
                        return;
                    }
                    names.insert(name);
                    total += n;
                    if (r == 0)
                    {
                        primary = name;
                        primaryLevel = n;
                        sub = sc;
                    }
                    else
                        extra.append(QJsonObject{{"name", name}, {"level", n}, {"subclass", sc}});
                }
                if (total > 20)
                {
                    QMessageBox::warning(&dialog, "Classes",
                                         "Total character level must be 20 or less.");
                    return;
                }
                bool changed = primary != text("class");
                character["class"] = primary;
                character["primaryLevel"] = primaryLevel;
                character["subclass"] = sub;
                character["multiclasses"] = extra;
                character["level"] = total;
                loadWidgets();
                if (changed)
                    classDefaults();
                put("level", total);
                dialog.accept();
            });
        dialog.exec();
    }
    void chooseLanguages()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("Choose languages");
        dialog.resize(420, 570);
        QVBoxLayout layout(&dialog);
        QLabel note("Select every language your character knows. Normally choose Common and two "
                    "standard languages; rare languages need a feature or DM permission.");
        note.setWordWrap(true);
        layout.addWidget(&note);
        QListWidget list;
        layout.addWidget(&list);
        QStringList options = {"Common",      "Common Sign Language",
                               "Draconic",    "Dwarvish",
                               "Elvish",      "Giant",
                               "Gnomish",     "Goblin",
                               "Halfling",    "Orc",
                               "Abyssal",     "Celestial",
                               "Deep Speech", "Druidic",
                               "Infernal",    "Primordial",
                               "Sylvan",      "Thieves’ Cant",
                               "Undercommon", "Aquan",
                               "Auran",       "Ignan",
                               "Terran"};
        QStringList selected =
            text("languages").split(QRegularExpression("[,;\\n]"), Qt::SkipEmptyParts);
        for (auto &v : selected)
            v = v.trimmed();
        for (auto v : selected)
            if (!options.contains(v))
                options << v;
        for (int i = 0; i < options.size(); ++i)
        {
            auto *item = new QListWidgetItem(options[i], &list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(options[i]) ? Qt::Checked : Qt::Unchecked);
            item->setToolTip(i < 10   ? "Standard language"
                             : i < 19 ? "Rare / class language"
                                      : "Primordial dialect or existing custom language");
        }
        QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout.addWidget(&actions);
        connect(&actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        QStringList result;
        for (int i = 0; i < list.count(); ++i)
            if (list.item(i)->checkState() == Qt::Checked)
                result << list.item(i)->text();
        qobject_cast<QLineEdit *>(fields["languages"])->setText(result.join(", "));
    }
    void applyHpChange(int amount, bool healing)
    {
        if (healing)
        {
            setNumber("hpCurrent", qMin(maximumHp(), number("hpCurrent") + amount));
            if (number("hpCurrent") > 0)
            {
                setNumber("deathSuccess", 0);
                setNumber("deathFails", 0);
            }
        }
        else
        {
            int absorbed = qMin(amount, number("hpTemp"));
            int remaining = amount - absorbed;
            if (number("hpCurrent") == 0 && remaining > 0)
            {
                setNumber("deathFails", qMin(3, number("deathFails") + 1));
                setNumber("deathSuccess", 0);
            }
            setNumber("hpTemp", number("hpTemp") - absorbed);
            setNumber("hpCurrent", qMax(0, number("hpCurrent") - remaining));
        }
        log(healing ? "Healing" : "Damage",
            QString::number(amount) + "; current HP " + QString::number(number("hpCurrent")));
    }
    void hpChange(bool healing)
    {
        bool okay = false;
        int amount = QInputDialog::getInt(
            this, healing ? "Healing" : "Take damage",
            healing ? "Hit points regained:" : "Damage after resistance or other adjustments:", 1,
            1, 9999, 1, &okay);
        if (okay)
            applyHpChange(amount, healing);
    }
    void applyDeathSave(int n, int total = -9999)
    {
        if (total == -9999)
            total = n;
        if (n == 20)
        {
            setNumber("hpCurrent", 1);
            setNumber("deathSuccess", 0);
            setNumber("deathFails", 0);
        }
        else if (n == 1)
            setNumber("deathFails", qMin(3, number("deathFails") + 2));
        else if (total >= 10)
        {
            setNumber("deathSuccess", qMin(3, number("deathSuccess") + 1));
            if (number("deathSuccess") >= 3)
                setNumber("deathFails", 0);
        }
        else
            setNumber("deathFails", qMin(3, number("deathFails") + 1));
        log("Death save", QString("d20 [%1]; %2 successes, %3 failures; modified total %4")
                              .arg(n)
                              .arg(number("deathSuccess"))
                              .arg(number("deathFails"))
                              .arg(total));
    }
    void deathSave()
    {
        if (number("hpCurrent") > 0)
        {
            displayError("Death saves are tracked at 0 hit points.");
            return;
        }
        if (number("deathFails") >= 3 || number("deathSuccess") >= 3)
        {
            displayError("This character has completed their death saves. Update the tracked state "
                         "before rolling again.");
            return;
        }
        auto result =
            d20("Death saving throw",
                effectBonus({"save:*", "deathSave"}) + pb() * effectTraining({"save:*"}, 0),
                {"save:*", "deathSave"});
        applyDeathSave(result.face, result.total);
    }
    void applyBackground()
    {
        if (character["abilitiesLocked"].toBool())
            return;
        if (character["backgroundApplied"].toBool())
        {
            displayError("Background increases have already been applied. Adjust the final scores "
                         "directly if your background changes.");
            return;
        }
        const auto choices = backgroundAbilityChoices();
        QDialog dialog(this);
        dialog.setWindowTitle("Apply background ability increases");
        QFormLayout form(&dialog);
        QComboBox mode, first, second, third;
        mode.setObjectName("backgroundIncreaseMode");
        mode.addItems({"+2 to one, +1 to another"});
        if (choices.size() >= 3)
            mode.addItem(choices.size() == 3 ? "+1 to all three background abilities"
                                             : "+1 to three different abilities");
        first.addItems(choices);
        second.addItems(choices);
        third.addItems(choices);
        third.setCurrentIndex(qMin(2, third.count() - 1));
        if (second.count() > 1)
            second.setCurrentIndex(1);
        form.addRow("Increase method", &mode);
        form.addRow("+2 ability", &first);
        form.addRow("+1 ability", &second);
        if (choices.size() > 3)
        {
            form.addRow("Third +1 ability", &third);
            third.setEnabled(false);
            connect(&mode, &QComboBox::currentIndexChanged, &dialog,
                    [&](int index)
                    {
                        third.setEnabled(index == 1);
                        qobject_cast<QLabel *>(form.labelForField(&first))
                            ->setText(index == 1 ? "First +1 ability" : "+2 ability");
                    });
        }
        QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form.addRow(&actions);
        connect(&actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        QMap<QString, int> increases;
        if (mode.currentIndex() == 0)
        {
            if (first.currentText() == second.currentText())
            {
                displayError("Choose two different abilities.");
                return;
            }
            increases[first.currentText()] = 2;
            increases[second.currentText()] = 1;
        }
        else
        {
            auto selected =
                choices.size() == 3
                    ? choices
                    : QStringList{first.currentText(), second.currentText(), third.currentText()};
            if (QSet<QString>(selected.begin(), selected.end()).size() != 3)
            {
                displayError("Choose three different abilities.");
                return;
            }
            for (const auto &a : selected)
                increases[a] = 1;
        }
        for (auto it = increases.begin(); it != increases.end(); ++it)
            if (number(it.key(), 10) + it.value() > 20)
            {
                displayError("A background increase cannot raise an ability above 20.");
                return;
            }
        QJsonObject allocation;
        for (auto it = increases.begin(); it != increases.end(); ++it)
        {
            character[it.key()] = number(it.key(), 10) + it.value();
            allocation[it.key()] = it.value();
        }
        character["backgroundIncreases"] = allocation;
        character["backgroundApplied"] = true;
        loadWidgets();
        put("backgroundIncreases", allocation);
    }
    void classDefaults()
    {
        if (loading)
            return;
        QMap<QString, QList<int>> saves{
            {"Artificer", {2, 3}}, {"Barbarian", {0, 2}}, {"Bard", {1, 5}},     {"Cleric", {4, 5}},
            {"Druid", {3, 4}},     {"Fighter", {0, 2}},   {"Monk", {0, 1}},     {"Paladin", {4, 5}},
            {"Ranger", {0, 1}},    {"Rogue", {1, 3}},     {"Sorcerer", {2, 5}}, {"Warlock", {4, 5}},
            {"Wizard", {3, 4}}};
        QJsonArray a;
        for (int i = 0; i < 6; ++i)
        {
            bool trained = saves.value(text("class")).contains(i);
            QSignalBlocker b(saveProf[i]);
            saveProf[i]->setChecked(trained);
            a.append(trained);
        }
        put("saveProfs", a);
        QMap<QString, QString> cast{{"Bard", "Charisma"},    {"Cleric", "Wisdom"},
                                    {"Druid", "Wisdom"},     {"Paladin", "Charisma"},
                                    {"Ranger", "Wisdom"},    {"Sorcerer", "Charisma"},
                                    {"Warlock", "Charisma"}, {"Wizard", "Intelligence"}};
        auto *combo = qobject_cast<QComboBox *>(fields["spellAbility"]);
        combo->setCurrentText(cast.value(text("class"), "Intelligence"));
    }
    QJsonObject findSpell(const QString &name) const
    {
        for (const auto &v : catalog["spells"].toArray())
            if (v.toObject()["name"].toString() == name)
                return v.toObject();
        return {};
    }
    int weaponAbility(const QJsonObject &w) const
    {
        return ability(qBound(0, w["ability"].toInt(), 5));
    }
    QString weaponDice(const QJsonObject &w) const
    {
        if (w["twoHanded"].toBool())
        {
            auto m =
                QRegularExpression("Versatile \\((\\dd\\d+)\\)").match(w["properties"].toString());
            if (m.hasMatch())
                return m.captured(1);
        }
        return w["dice"].toString();
    }
    void refreshWeapons()
    {
        if (!weaponTable)
            return;
        QSignalBlocker blocker(weaponTable);
        int selected = weaponTable->currentRow();
        weaponTable->setRowCount(0);
        for (const auto &v : array("weapons"))
        {
            auto w = v.toObject();
            int r = weaponTable->rowCount();
            weaponTable->insertRow(r);
            int bonus = weaponBonus(w, true);
            QStringList cells{w["name"].toString(),
                              sign(weaponBonus(w, false)) + effectDice(weaponTargets(w, false)),
                              weaponDice(w) + sign(bonus) + effectDice(weaponTargets(w, true)),
                              w["type"].toString(),
                              w["properties"].toString(),
                              w["mastery"].toString()};
            for (int c = 0; c < cells.size(); ++c)
                weaponTable->setItem(r, c, new QTableWidgetItem(cells[c]));
        }
        if (selected >= 0 && selected < weaponTable->rowCount())
            weaponTable->selectRow(selected);
    }
    void weaponDialog(bool editing = false)
    {
        int selected = weaponTable->currentRow();
        if (editing && selected < 0)
            return;
        QJsonObject existing = editing ? array("weapons").at(selected).toObject() : QJsonObject();
        QDialog dialog(this);
        dialog.setWindowTitle(editing ? "Edit weapon" : "Add weapon");
        dialog.setMinimumWidth(550);
        QFormLayout form(&dialog);
        QComboBox picker;
        picker.addItem("Custom weapon");
        for (const auto &v : catalog["weapons"].toArray())
            picker.addItem(v.toObject()["name"].toString());
        QLineEdit name, dice, type, properties, mastery;
        QComboBox stat;
        stat.addItems(abilities);
        QCheckBox ranged("Ranged weapon (not a thrown Melee weapon)");
        QCheckBox dueling("Dueling conditions: held in one hand, no other weapons held");
        QCheckBox thrown("Using a ranged throw with a Thrown-property weapon");
        QCheckBox trained("Add proficiency bonus to attacks"),
            two("Use the versatile two-handed damage die");
        QSpinBox magic;
        magic.setRange(-10, 10);
        form.addRow("Weapon", &picker);
        form.addRow("Name", &name);
        form.addRow("Damage dice", &dice);
        form.addRow("Damage type", &type);
        form.addRow("Properties", &properties);
        form.addRow("Mastery", &mastery);
        form.addRow("Attack / damage ability", &stat);
        form.addRow("Magic bonus", &magic);
        form.addRow(&ranged);
        form.addRow(&dueling);
        form.addRow(&thrown);
        form.addRow(&trained);
        form.addRow(&two);
        auto fill = [&](const QJsonObject &w)
        {
            name.setText(w["name"].toString());
            dice.setText(w["dice"].toString());
            type.setText(w["type"].toString());
            properties.setText(w["properties"].toString());
            mastery.setText(w["mastery"].toString());
            ranged.setChecked(rangedWeapon(w));
        };
        connect(&picker, &QComboBox::currentIndexChanged, &dialog,
                [&](int index)
                {
                    if (index < 1)
                        return;
                    auto w = catalog["weapons"].toArray().at(index - 1).toObject();
                    fill(w);
                    bool ranged = w["category"].toString().contains("Ranged");
                    bool finesse = w["properties"].toString().contains("Finesse");
                    stat.setCurrentIndex(ranged || (finesse && ability(1) > ability(0)) ? 1 : 0);
                    QString cls = text("class");
                    bool martial =
                        QStringList{"Barbarian", "Fighter", "Paladin", "Ranger"}.contains(cls);
                    bool simple = w["category"].toString().startsWith("Simple");
                    bool rogue =
                        cls == "Rogue" && (finesse || w["properties"].toString().contains("Light"));
                    bool monk = cls == "Monk" && w["properties"].toString().contains("Light");
                    trained.setChecked(simple || martial || rogue || monk);
                });
        if (editing)
        {
            fill(existing);
            stat.setCurrentIndex(existing["ability"].toInt());
            magic.setValue(existing["magic"].toInt());
            trained.setChecked(existing["trained"].toBool());
            two.setChecked(existing["twoHanded"].toBool());
            dueling.setChecked(existing["dueling"].toBool());
            thrown.setChecked(existing["thrown"].toBool());
        }
        else
            picker.setCurrentIndex(1);
        QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form.addRow(&actions);
        connect(&actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (name.text().trimmed().isEmpty())
        {
            displayError("Give the weapon a name.");
            return;
        }
        if (!QRegularExpression("^(?:[1-9][0-9]?d[1-9][0-9]{0,2}|[1-9][0-9]?)$")
                 .match(dice.text().trimmed())
                 .hasMatch())
        {
            displayError("Use a base weapon die such as 1d8, 2d6, or a fixed value such as 1.");
            return;
        }
        QJsonObject w{{"name", name.text().trimmed()},
                      {"dice", dice.text().trimmed()},
                      {"type", type.text()},
                      {"properties", properties.text()},
                      {"mastery", mastery.text()},
                      {"ability", stat.currentIndex()},
                      {"magic", magic.value()},
                      {"trained", trained.isChecked()},
                      {"twoHanded", two.isChecked()},
                      {"ranged", ranged.isChecked()},
                      {"dueling", dueling.isChecked()},
                      {"thrown", thrown.isChecked()}};
        auto rows = array("weapons");
        if (editing)
            rows[selected] = w;
        else
            rows.append(w);
        put("weapons", rows);
        refreshWeapons();
    }
    void weaponRoll(bool damage)
    {
        int row = weaponTable->currentRow();
        if (row < 0)
        {
            displayError("Select a weapon first.");
            return;
        }
        auto w = array("weapons").at(row).toObject();
        int bonus = weaponBonus(w, true);
        if (!damage)
        {
            d20(w["name"].toString() + " attack", weaponBonus(w, false), weaponTargets(w, false));
            return;
        }
        try
        {
            auto result = rollDice(weaponDice(w) + sign(bonus) + effectDice(weaponTargets(w, true)),
                                   criticalDamage->isChecked());
            log(w["name"].toString() + " damage",
                QString("%1 = %2 %3%4")
                    .arg(result.detail)
                    .arg(qMax(0, result.total))
                    .arg(w["type"].toString(),
                         criticalDamage->isChecked() ? " (critical dice doubled)" : ""));
        }
        catch (const std::exception &e)
        {
            displayError(QString::fromUtf8(e.what()));
        }
    }
    void refreshSpells()
    {
        if (!spellTable)
            return;
        int selected = spellTable->currentRow();
        spellTable->setRowCount(0);
        for (const auto &v : array("spells"))
        {
            auto s = v.toObject();
            auto ref = findSpell(s["name"].toString());
            QStringList dice;
            for (const auto &d : ref["dice"].toArray())
                dice << d.toString();
            int row = spellTable->rowCount();
            spellTable->insertRow(row);
            QStringList cells{s["name"].toString(),
                              ref["level"].toInt() == 0 ? "Cantrip"
                                                        : QString::number(ref["level"].toInt()),
                              dice.isEmpty() ? "No dice in effect" : dice.join(", "),
                              s["prepared"].toBool() ? "Yes" : "No"};
            for (int c = 0; c < cells.size(); ++c)
                spellTable->setItem(row, c, new QTableWidgetItem(cells[c]));
        }
        if (selected >= 0 && selected < spellTable->rowCount())
            spellTable->selectRow(selected);
    }
    void addSpell()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("Spell reference — SRD 5.2.1");
        dialog.resize(950, 680);
        QVBoxLayout layout(&dialog);
        QLineEdit filter;
        filter.setPlaceholderText("Search spell name, class, level, or school");
        layout.addWidget(&filter);
        QSplitter splitter;
        QListWidget list;
        QPlainTextEdit details;
        details.setReadOnly(true);
        splitter.addWidget(&list);
        splitter.addWidget(&details);
        splitter.setStretchFactor(1, 2);
        layout.addWidget(&splitter, 1);
        QCheckBox prepared("Mark as prepared");
        prepared.setChecked(true);
        layout.addWidget(&prepared);
        QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        actions.button(QDialogButtonBox::Ok)->setText("Add to spellbook");
        layout.addWidget(&actions);
        auto populate = [&]
        {
            list.clear();
            for (const auto &v : catalog["spells"].toArray())
            {
                auto s = v.toObject();
                if (s["legacyPdf"].toBool() && !character["allowLegacyPdfRules"].toBool())
                    continue;
                if (!(s["name"].toString() + " " + s["meta"].toString())
                         .contains(filter.text(), Qt::CaseInsensitive))
                    continue;
                auto *item = new QListWidgetItem(s["name"].toString(), &list);
                item->setToolTip(s["meta"].toString());
            }
            if (list.count())
                list.setCurrentRow(0);
            else
                details.clear();
        };
        connect(&list, &QListWidget::currentTextChanged, &dialog, [&](const QString &name)
                { details.setPlainText(sourcesLast(findSpell(name)["description"].toString())); });
        connect(&filter, &QLineEdit::textChanged, &dialog, [&] { populate(); });
        connect(&actions, &QDialogButtonBox::accepted, &dialog,
                [&]
                {
                    if (list.currentItem())
                        dialog.accept();
                });
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        populate();
        if (dialog.exec() != QDialog::Accepted)
            return;
        QString name = list.currentItem()->text();
        auto spells = array("spells");
        for (const auto &v : spells)
            if (v.toObject()["name"].toString() == name)
            {
                displayError("That spell is already in this character’s spellbook.");
                return;
            }
        spells.append(QJsonObject{{"name", name}, {"prepared", prepared.isChecked()}});
        put("spells", spells);
        refreshSpells();
        spellTable->selectRow(spellTable->rowCount() - 1);
    }
    void populateSlots()
    {
        QSignalBlocker blocker(slotTable);
        slotTable->setRowCount(0);
        slotTable->setRowCount(9);
        auto max = array("slotsMax"), used = array("slotsUsed");
        for (int i = 0; i < 9; ++i)
        {
            slotTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
            auto *m = new QSpinBox;
            auto *u = new QSpinBox;
            m->setRange(0, 99);
            u->setRange(0, 99);
            m->setValue(max.at(i).toInt());
            u->setValue(used.at(i).toInt());
            slotTable->setCellWidget(i, 1, m);
            slotTable->setCellWidget(i, 2, u);
            auto changed = [this]
            {
                if (loading)
                    return;
                QJsonArray maximum, spent;
                for (int r = 0; r < 9; ++r)
                {
                    maximum.append(qobject_cast<QSpinBox *>(slotTable->cellWidget(r, 1))->value());
                    spent.append(qobject_cast<QSpinBox *>(slotTable->cellWidget(r, 2))->value());
                }
                put("slotsMax", maximum);
                put("slotsUsed", spent);
            };
            connect(m, &QSpinBox::valueChanged, this, changed);
            connect(u, &QSpinBox::valueChanged, this, changed);
        }
    }
    void configureSlots(bool confirm = true)
    {
        if (confirm &&
            QMessageBox::question(this, "Configure slots",
                                  "Calculate spellcasting and separate Pact Magic slots from all "
                                  "classes and reset spent slots?") != QMessageBox::Yes)
            return;
        static const int full[20][9] = {{2},
                                        {3},
                                        {4, 2},
                                        {4, 3},
                                        {4, 3, 2},
                                        {4, 3, 3},
                                        {4, 3, 3, 1},
                                        {4, 3, 3, 2},
                                        {4, 3, 3, 3, 1},
                                        {4, 3, 3, 3, 2},
                                        {4, 3, 3, 3, 2, 1},
                                        {4, 3, 3, 3, 2, 1},
                                        {4, 3, 3, 3, 2, 1, 1},
                                        {4, 3, 3, 3, 2, 1, 1},
                                        {4, 3, 3, 3, 2, 1, 1, 1},
                                        {4, 3, 3, 3, 2, 1, 1, 1},
                                        {4, 3, 3, 3, 2, 1, 1, 1, 1},
                                        {4, 3, 3, 3, 3, 1, 1, 1, 1},
                                        {4, 3, 3, 3, 3, 2, 1, 1, 1},
                                        {4, 3, 3, 3, 3, 2, 2, 1, 1}};
        int effective = 0, casters = 0, thirdLevel = 0, warlock = 0;
        for (auto v : classRoster())
        {
            auto c = v.toObject();
            QString cls = c["name"].toString();
            int n = c["level"].toInt();
            if (QStringList{"Bard", "Cleric", "Druid", "Sorcerer", "Wizard"}.contains(cls))
            {
                effective += n;
                ++casters;
            }
            else if (cls == "Paladin" || cls == "Ranger" || cls == "Artificer")
            {
                effective += (n + 1) / 2;
                ++casters;
            }
            else if (isThirdCaster(cls))
            {
                if (n >= 3)
                {
                    thirdLevel = n;
                    effective += n / 3;
                    ++casters;
                }
            }
            else if (cls == "Warlock")
                warlock = n;
        }
        if (casters == 1 && thirdLevel)
            effective = (thirdLevel + 2) / 3;
        effective = qBound(0, effective, 20);
        setNumber("pactLevel", warlock ? qMin(5, (warlock + 1) / 2) : 1);
        setNumber("pactMax", warlock ? (warlock == 1   ? 1
                                        : warlock < 11 ? 2
                                        : warlock < 17 ? 3
                                                       : 4)
                                     : 0);
        setNumber("pactUsed", 0);
        QJsonArray maximum, used;
        for (int i = 0; i < 9; ++i)
        {
            maximum.append(effective ? full[effective - 1][i] : 0);
            used.append(0);
        }
        put("slotsMax", maximum);
        put("slotsUsed", used);
        populateSlots();
    }
    QString spellFormula(const QString &name, int slot, QString castingAbility = {}) const
    {
        QString base = baseSpellFormula(name, slot, castingAbility);
        bool healing = name == "Cure Wounds" || name == "Healing Word" ||
                       name == "Mass Cure Wounds" || name == "Mass Healing Word" ||
                       name == "Heal" || name == "Mass Heal" || name == "Power Word Heal" ||
                       name == "Prayer Of Healing" || name == "Aura Of Vitality";
        QStringList targets{healing ? "spellHealing" : "spellDamage"};
        bool damage =
            findSpell(name)["description"].toString().contains("damage", Qt::CaseInsensitive);
        if (!base.isEmpty() && (healing || damage))
        {
            int bonus = effectBonus(targets);
            if (damage && findSpell(name)["level"].toInt() == 0 &&
                (castingAbility.isEmpty() ? text("spellAbility") : castingAbility) == "Wisdom")
            {
                auto choices = character["classChoices"].toObject();
                for (auto cls : QStringList{"Cleric", "Druid"})
                    if (classLevel(cls) >= 7 && findSpell(name)["meta"].toString().contains(cls) &&
                        choices[cls + (cls == "Cleric" ? ":Blessed Strikes" : ":Elemental Fury")] ==
                            "Potent Spellcasting")
                    {
                        bonus += ability(4);
                        break;
                    }
            }
            if (bonus)
                base += sign(bonus);
            base += effectDice(targets);
        }
        return base;
    }
    QString baseSpellFormula(const QString &name, int slot, QString castingAbility = {}) const
    {
        auto uploaded = findSpell(name);
        if (uploaded["legacyPdf"].toBool())
            return uploadedSpellFormula(uploaded, slot, castingAbility);
        QMap<QString, QString> cantrips{
            {"Acid Splash", "d6"},     {"Fire Bolt", "d10"},      {"Ray Of Frost", "d8"},
            {"Sacred Flame", "d8"},    {"Chill Touch", "d10"},    {"Shocking Grasp", "d8"},
            {"Poison Spray", "d12"},   {"Produce Flame", "d8"},   {"Thorn Whip", "d6"},
            {"Vicious Mockery", "d6"}, {"Sorcerous Burst", "d8"}, {"Starry Wisp", "d8"}};
        int level = number("level", 1);
        int tier = 1 + (level >= 5) + (level >= 11) + (level >= 17);
        if (cantrips.contains(name))
            return QString::number(tier) + cantrips[name];
        int casting =
            abilities.indexOf(castingAbility.isEmpty() ? text("spellAbility") : castingAbility);
        if (casting < 0)
            casting = 3;
        int mod = ability(casting);
        if (name == "Cure Wounds")
            return QString::number(2 * qMax(1, slot)) + "d8" + sign(mod);
        if (name == "Healing Word")
            return QString::number(2 * qMax(1, slot)) + "d4" + sign(mod);
        if (name == "Fireball" || name == "Lightning Bolt")
            return QString::number(8 + qMax(0, slot - 3)) + "d6";
        if (name == "Burning Hands")
            return QString::number(3 + qMax(0, slot - 1)) + "d6";
        if (name == "Thunderwave")
            return QString::number(2 + qMax(0, slot - 1)) + "d8";
        if (name == "Guiding Bolt")
            return QString::number(4 + qMax(0, slot - 1)) + "d6";
        if (name == "Divine Smite")
            return QString::number(2 + qMax(0, slot - 1)) + "d8";
        if (name == "Magic Missile")
            return "1d4+1";
        if (name == "Eldritch Blast")
            return "1d10";
        auto dice = findSpell(name)["dice"].toArray();
        return dice.isEmpty() ? QString() : dice.first().toString();
    }
    QString selectedSpellAbility() const
    {
        int row = spellTable->currentRow();
        return row < 0 ? text("spellAbility")
                       : array("spells").at(row).toObject()["castingAbility"].toString(
                             text("spellAbility"));
    }
    void setSpellAbility()
    {
        int row = spellTable->currentRow();
        if (row < 0)
        {
            displayError("Select a spell first.");
            return;
        }
        bool ok = false;
        QString value =
            QInputDialog::getItem(this, "Spellcasting ability", "Ability for this spell", abilities,
                                  qMax(0, abilities.indexOf(selectedSpellAbility())), false, &ok);
        if (!ok)
            return;
        auto spells = array("spells");
        auto entry = spells.at(row).toObject();
        entry["castingAbility"] = value;
        spells[row] = entry;
        put("spells", spells);
        refreshSpells();
        spellTable->selectRow(row);
    }
    void castSpell()
    {
        int row = spellTable->currentRow();
        if (row < 0)
        {
            displayError("Select a spell first.");
            return;
        }
        QString name = array("spells").at(row).toObject()["name"].toString();
        auto ref = findSpell(name);
        int base = ref["level"].toInt();
        QDialog dialog(this);
        dialog.setWindowTitle("Cast / roll — " + name);
        QFormLayout form(&dialog);
        QSpinBox slot;
        slot.setRange(base == 0 ? 0 : base, base == 0 ? 0 : 9);
        slot.setValue(base);
        QComboBox pool;
        pool.addItems({"Spellcasting slots", "Pact Magic slots"});
        pool.setEnabled(base > 0);
        form.addRow("Slot pool", &pool);
        connect(&pool, &QComboBox::currentIndexChanged, &dialog,
                [&](int index)
                {
                    if (index == 1)
                    {
                        slot.setRange(number("pactLevel", 1), number("pactLevel", 1));
                    }
                    else
                        slot.setRange(base == 0 ? 0 : base, base == 0 ? 0 : 9);
                });
        QComboBox castingAbility;
        castingAbility.addItems(abilities);
        castingAbility.setCurrentText(selectedSpellAbility());
        form.addRow("Spellcasting ability", &castingAbility);
        QLineEdit expression(spellFormula(name, base, castingAbility.currentText()));
        connect(&castingAbility, &QComboBox::currentTextChanged, &dialog, [&](const QString &value)
                { expression.setText(spellFormula(name, slot.value(), value)); });
        QCheckBox spend("Spend one slot");
        spend.setChecked(base > 0);
        spend.setEnabled(base > 0);
        QLabel note("Confirm the correct effect and dice using the spell description. For multiple "
                    "rays, beams, or darts, this rolls one instance. Adjust dice for other scaling "
                    "or character features.");
        note.setWordWrap(true);
        note.setMaximumWidth(520);
        form.addRow("Cast at slot level", &slot);
        QComboBox effectChoice;
        const auto choices = ref["rollOptions"].toArray();
        for (auto value : choices)
            effectChoice.addItem(value.toObject()["label"].toString(),
                                 value.toObject()["dice"].toString());
        if (!choices.isEmpty())
            form.addRow("Effect to roll", &effectChoice);
        auto selectEffect = [&]
        {
            if (!choices.isEmpty())
                expression.setText(uploadedSpellFormula(ref, slot.value(),
                                                        castingAbility.currentText(),
                                                        effectChoice.currentData().toString()));
        };
        connect(&effectChoice, &QComboBox::currentIndexChanged, &dialog, [&] { selectEffect(); });
        connect(&slot, &QSpinBox::valueChanged, &dialog, [&] { selectEffect(); });
        connect(&castingAbility, &QComboBox::currentTextChanged, &dialog, [&] { selectEffect(); });
        selectEffect();
        form.addRow("Effect dice (editable)", &expression);
        form.addRow(&spend);
        form.addRow(&note);
        connect(&slot, &QSpinBox::valueChanged, &dialog,
                [&](int value)
                {
                    if (choices.isEmpty())
                        expression.setText(spellFormula(name, value, castingAbility.currentText()));
                    else
                        selectEffect();
                });
        QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form.addRow(&actions);
        connect(&actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        Roll result;
        bool hasDice = !expression.text().trimmed().isEmpty();
        try
        {
            if (hasDice)
                result = rollDice(expression.text());
        }
        catch (const std::exception &e)
        {
            displayError(QString::fromUtf8(e.what()));
            return;
        }
        if (spend.isChecked() && pool.currentIndex() == 1)
        {
            if (slot.value() < base || number("pactUsed") >= number("pactMax"))
            {
                displayError("No suitable Pact Magic slot remains.");
                return;
            }
            setNumber("pactUsed", number("pactUsed") + 1);
        }
        else if (spend.isChecked())
        {
            int index = slot.value() - 1;
            auto maximum = array("slotsMax"), used = array("slotsUsed");
            if (maximum.at(index).toInt() <= used.at(index).toInt())
            {
                displayError("No slot of that level remains. Set your slot maximums or choose a "
                             "different slot level.");
                return;
            }
            while (used.size() < 9)
                used.append(0);
            used[index] = used[index].toInt() + 1;
            put("slotsUsed", used);
            populateSlots();
        }
        log(name + (base == 0 ? " (cantrip)" : QString(" (slot %1)").arg(slot.value())),
            hasDice ? result.detail + " = " + QString::number(qMax(0, result.total))
                    : "Effect recorded; no dice rolled.");
    }
    void populateInventory()
    {
        QSignalBlocker blocker(inventoryTable);
        inventoryTable->setRowCount(0);
        for (const auto &v : array("inventory"))
        {
            auto o = v.toObject();
            int row = inventoryTable->rowCount();
            inventoryTable->insertRow(row);
            QStringList cells{o["name"].toString(), o["qty"].toString(), o["weight"].toString(),
                              o["notes"].toString()};
            for (int c = 0; c < 4; ++c)
                inventoryTable->setItem(row, c, new QTableWidgetItem(cells[c]));
            auto *equipped = new QTableWidgetItem;
            equipped->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            equipped->setCheckState(o["equipped"].toBool() ? Qt::Checked : Qt::Unchecked);
            inventoryTable->setItem(row, 4, equipped);
            equipped->setToolTip(effectSummary(o));
            inventoryTable->setItem(row, 5, new QTableWidgetItem(o["cost"].toString()));
            inventoryTable->item(row, 3)->setToolTip(o["notes"].toString());
        }
    }
    void inventoryChanged()
    {
        if (loading)
            return;
        QJsonArray rows;
        auto previous = array("inventory");
        for (int r = 0; r < inventoryTable->rowCount(); ++r)
        {
            QJsonObject o = previous.at(r).toObject();
            const QStringList keys{"name", "qty", "weight", "notes"};
            for (int c = 0; c < 4; ++c)
                o[keys[c]] =
                    inventoryTable->item(r, c) ? inventoryTable->item(r, c)->text() : QString();
            o["cost"] = inventoryTable->item(r, 5) ? inventoryTable->item(r, 5)->text() : QString();
            o["equipped"] = inventoryTable->item(r, 4) &&
                            inventoryTable->item(r, 4)->checkState() == Qt::Checked;
            rows.append(o);
        }
        put("inventory", rows);
    }
    void updateWeight()
    {
        refreshCarryingCapacity();
    }
    void removeEntry(const QString &key, QTableWidget *view)
    {
        int row = view->currentRow();
        if (row < 0)
            return;
        auto entries = array(key);
        entries.removeAt(row);
        put(key, entries);
        if (key == "weapons")
            refreshWeapons();
        else if (key == "spells")
            refreshSpells();
        else if (key == "inventory")
            populateInventory();
    }
    void exportCharacter()
    {
        if (!save())
        {
            displayError("Save failed. You can still export the draft below.");
        }
        QString filename = QFileDialog::getSaveFileName(
            this, "Export character backup",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/character.json",
            "Character backup (*.json)");
        if (filename.isEmpty())
            return;
        QSaveFile f(filename);
        auto bytes = QJsonDocument(character).toJson();
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
            displayError("Could not export the character.");
    }
    void importCharacter()
    {
        QString filename = QFileDialog::getOpenFileName(this, "Import character backup", {},
                                                        "Character backup (*.json)");
        if (filename.isEmpty())
            return;
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 5 * 1024 * 1024)
        {
            displayError("Could not read this character backup, or the file is too large.");
            return;
        }
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(f.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject() ||
            doc.object()["schema"].toInt() != 1 || !doc.object()["name"].isString())
        {
            displayError("Choose a character backup exported by Adventurer’s Ledger.");
            return;
        }
        if (!save())
        {
            displayError("Save the current character before importing.");
            return;
        }
        auto imported = blank();
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            imported[it.key()] = it.value();
        current = QUuid::createUuid().toString(QUuid::WithoutBraces);
        character = imported;
        loadWidgets();
        dirty = true;
        save();
        refreshLibrary();
    }
    void buildUi()
    {
        auto *root = new QWidget;
        auto *outer = new QHBoxLayout(root);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        setCentralWidget(root);
        auto *sidebar = new QWidget;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(235);
        auto *side = new QVBoxLayout(sidebar);
        side->setContentsMargins(20, 25, 20, 20);
        side->setSpacing(14);
        auto *brand = new QLabel("ADVENTURER’S\nLEDGER");
        brand->setObjectName("brand");
        side->addWidget(brand);
        auto *edition = new QLabel("2024 RULES · SRD 5.2.1");
        edition->setObjectName("muted");
        side->addWidget(edition);
        side->addWidget(button("+ New character", [this] { newCharacter(); }));
        search = new QLineEdit;
        search->setPlaceholderText("Find a character…");
        side->addWidget(search);
        characters = new QListWidget;
        side->addWidget(characters, 1);
        side->addWidget(button("Import character", [this] { importCharacter(); }));
        side->addWidget(button("Export character", [this] { exportCharacter(); }));
        saveState = new QLabel("Saved locally");
        saveState->setWordWrap(true);
        side->addWidget(saveState);
        outer->addWidget(sidebar);
        auto *main = new QWidget;
        auto *layout = new QVBoxLayout(main);
        layout->setContentsMargins(24, 20, 24, 16);
        QFile advancementFile(":/advancement.json");
        if (advancementFile.open(QIODevice::ReadOnly))
            advancementCatalog =
                QJsonDocument::fromJson(advancementFile.readAll()).object()["classes"].toObject();
        advancementCatalog["Artificer"] = artificerReference()["classReference"];
        for (auto value : supplementalSubclassCatalog())
        {
            auto entry = value.toObject();
            auto &names = subclasses[entry["className"].toString()];
            if (!names.contains(entry["name"].toString()))
                names << entry["name"].toString();
        }
        QFile equipmentFile(":/equipment.json");
        if (equipmentFile.open(QIODevice::ReadOnly))
            equipmentCatalog =
                QJsonDocument::fromJson(equipmentFile.readAll()).object()["items"].toArray();
        QFile speciesFile(":/species.json");
        if (speciesFile.open(QIODevice::ReadOnly))
            speciesCatalog =
                QJsonDocument::fromJson(speciesFile.readAll()).object()["species"].toArray();
        QFile featFile(":/feats.json");
        if (featFile.open(QIODevice::ReadOnly))
            featCatalog = QJsonDocument::fromJson(featFile.readAll()).object()["feats"].toArray();
        for (int i = 0; i < featCatalog.size(); ++i)
        {
            auto f = featCatalog[i].toObject();
            f["source"] = "Player's Handbook (2024)";
            featCatalog[i] = f;
        }
        QFile supplementalFeats(":/supplemental_feats.json");
        if (supplementalFeats.open(QIODevice::ReadOnly))
            for (auto f :
                 QJsonDocument::fromJson(supplementalFeats.readAll()).object()["feats"].toArray())
                featCatalog.append(f);
        QFile backgrounds(":/backgrounds.json");
        if (backgrounds.open(QIODevice::ReadOnly))
            backgroundCatalog =
                QJsonDocument::fromJson(backgrounds.readAll()).object()["backgrounds"].toArray();
        summary = new QLabel;
        summary->setObjectName("summary");
        layout->addWidget(summary);
        auto *rollbar = new QHBoxLayout;
        rollbar->addWidget(new QLabel("D20 mode"));
        rollMode = new QComboBox;
        rollMode->addItems({"Normal", "Advantage", "Disadvantage"});
        rollbar->addWidget(rollMode);
        rollbar->addStretch();
        rollbar->addWidget(new QLabel("DICE"));
        diceInput = new QComboBox;
        diceInput->setEditable(true);
        diceInput->setInsertPolicy(QComboBox::NoInsert);
        diceInput->addItems({"1d4", "1d6", "1d8", "1d10", "1d12", "1d20", "1d100"});
        diceInput->setCurrentText("1d20");
        diceInput->setAccessibleName("Dice selection or custom roll");
        diceInput->setToolTip("Choose a die or type a custom roll, such as 2d6+3.");
        diceInput->setMinimumWidth(110);
        diceInput->setMaximumWidth(180);
        rollbar->addWidget(diceInput);
        diceVisual = new RollingDie;
        rollbar->addWidget(diceVisual);
        auto roll = [this]
        {
            if (diceVisual->rolling())
                return;
            try
            {
                if (diceInput->currentText().trimmed().startsWith("1d20") &&
                    rollMode->currentIndex() > 0)
                {
                    auto m = QRegularExpression("^1d20([+-][0-9]+)?$")
                                 .match(diceInput->currentText().remove(' '));
                    if (m.hasMatch())
                    {
                        auto result = d20("Dice roll", m.captured(1).toInt());
                        diceVisual->animate(result);
                        return;
                    }
                }
                auto r = rollDice(diceInput->currentText());
                log("Dice roll", r.detail + " = " + QString::number(r.total));
                diceVisual->animate(r);
            }
            catch (const std::exception &e)
            {
                displayError(QString::fromUtf8(e.what()));
            }
        };
        auto *rollButton = button("Roll", roll);
        rollButton->setObjectName("topDiceRoll");
        rollbar->addWidget(rollButton);
        connect(diceInput->lineEdit(), &QLineEdit::returnPressed, this, roll);
        layout->addLayout(rollbar);
        tabs = new QTabWidget;
        layout->addWidget(tabs, 1);
        outer->addWidget(main, 1);
        QVBoxLayout *body;
        auto *sheet = page(body);
        auto *identity = new QGroupBox("CHARACTER");
        auto *identityForm = new QFormLayout(identity);
        identityForm->addRow("Name", line("name"));
        QStringList speciesNames;
        for (auto entry : speciesCatalog)
            speciesNames << entry.toObject()["name"].toString();
        speciesNames.sort();
        identityForm->addRow("Species / race", choice("species", speciesNames, true));
        identityForm->addRow(button("Species traits & lineage…", [this] { configureSpecies(); }));
        creationStatus = new QLabel;
        creationStatus->setWordWrap(true);
        identityForm->addRow(creationStatus);
        identityForm->addRow(button("Required starting proficiencies…",
                                    [this] { configureStartingProficiencies(); }));
        identityForm->addRow("Gender",
                             choice("gender", {"", "Female", "Male", "Nonbinary", "Other"}, true));
        auto *cls = choice("class", classes);
        identityForm->addRow("Starting class", cls);
        cls->setEnabled(false);
        identityForm->addRow(button("Manage classes / multiclass", [this] { editClasses(); }));
        identityForm->addRow("Subclass",
                             choice("subclass", QStringList{""} + subclasses.value("Fighter")));
        identityForm->addRow(
            button("Subclass features & choices…", [this] { editSubclassFeatures(); }));
        identityForm->addRow("Background", choice("background", backgroundNames(), true));
        identityForm->addRow(
            button("Browse backgrounds & book details…", [this] { browseBackgrounds(); }));
        identityForm->addRow(button("Grant background feat…", [this] { grantBackgroundFeat(); }));
        auto *totalLevel = spin("level", 1, 20);
        totalLevel->setReadOnly(true);
        totalLevel->setButtonSymbols(QAbstractSpinBox::NoButtons);
        identityForm->addRow("Total level", totalLevel);
        identityForm->addRow(
            "Alignment", choice("alignment", {"", "Lawful Good", "Neutral Good", "Chaotic Good",
                                              "Lawful Neutral", "True Neutral", "Chaotic Neutral",
                                              "Lawful Evil", "Neutral Evil", "Chaotic Evil"}));
        auto *deitySelector = choice("deity", deityNames(), true);
        deitySelector->setObjectName("deitySelector");
        deitySelector->setInsertPolicy(QComboBox::NoInsert);
        deitySelector->setToolTip("Choose a god or type a custom deity. Available to every class.");
        identityForm->addRow("Deity / faith", deitySelector);
        identityForm->addRow(button("Choose deity / browse gods…", [this] { browseDeities(); }));
        deityDetails = new QLabel;
        deityDetails->setWordWrap(true);
        deityDetails->setObjectName("deityDescription");
        identityForm->addRow(deityDetails);
        connect(deitySelector, &QComboBox::currentTextChanged, this,
                [this] { refreshDeityDescription(); });
        auto *faith = line("faithNotes");
        faith->setPlaceholderText("Beliefs, traditions, multiple gods or a custom pantheon…");
        identityForm->addRow("Faith notes", faith);
        auto *languageSummary = line("languages");
        languageSummary->setReadOnly(true);
        identityForm->addRow("Languages", languageSummary);
        identityForm->addRow(button("Choose languages…", [this] { chooseLanguages(); }));
        body->addWidget(identity);
        body->addWidget(
            button("Open feats & bonuses", [this] { tabs->setCurrentWidget(featPage); }));
        auto *abilityBox = new QGroupBox("ABILITIES AND SAVING THROWS");
        auto *abilityGrid = new QGridLayout(abilityBox);
        abilityGrid->addWidget(new QLabel("Ability"), 0, 0);
        abilityGrid->addWidget(new QLabel("Base score"), 0, 1);
        abilityGrid->addWidget(new QLabel("Total / modifier"), 0, 2);
        abilityGrid->addWidget(new QLabel("Save prof."), 0, 3);
        abilityGrid->addWidget(new QLabel("Save bonus"), 0, 4);
        for (int i = 0; i < 6; ++i)
        {
            abilityGrid->addWidget(new QLabel(abilities[i]), i + 1, 0);
            abilityGrid->addWidget(
                button(
                    "Details", [this, i]
                    { showRuleDescription(abilities[i], basicChoiceDescription(abilities[i])); }),
                i + 1, 7);
            abilityGrid->addWidget(spin(abilities[i], 1, 30), i + 1, 1);
            modLabels[i] = new QLabel;
            abilityGrid->addWidget(modLabels[i], i + 1, 2);
            saveProf[i] = new QCheckBox;
            abilityGrid->addWidget(saveProf[i], i + 1, 3);
            saveLabels[i] = new QLabel;
            abilityGrid->addWidget(saveLabels[i], i + 1, 4);
            abilityGrid->addWidget(button("Check",
                                          [this, i]
                                          {
                                              d20(abilities[i] + " check", checkBonus(i),
                                                  {"check:*", "check:" + abilities[i]});
                                          }),
                                   i + 1, 5);
            abilityGrid->addWidget(
                button("Save", [this, i]
                       { d20(abilities[i] + " saving throw", saveBonus(i), saveTargets(i)); }),
                i + 1, 6);
            connect(saveProf[i], &QCheckBox::toggled, this,
                    [this]
                    {
                        if (loading)
                            return;
                        QJsonArray a;
                        for (auto *w : saveProf)
                            a.append(w->isChecked());
                        put("saveProfs", a);
                    });
        }
        body->addWidget(abilityBox);
        auto *generation = new QHBoxLayout;
        generation->addWidget(
            button("Use standard array",
                   [this]
                   {
                       if (QMessageBox::question(
                               this, "Standard array",
                               "Replace starting scores and reset background increases?") ==
                           QMessageBox::Yes)
                           resetAbilityPool(false);
                   }));
        generation->addWidget(
            button("Roll 4d6, drop lowest",
                   [this]
                   {
                       if (QMessageBox::question(
                               this, "Roll ability scores",
                               "Generate six new scores and reset background increases?") ==
                           QMessageBox::Yes)
                           resetAbilityPool(true);
                   }));
        generation->addWidget(button("Assign ability scores…", [this] { assignAbilityPool(); }));
        generation->addWidget(button("Apply background increases", [this] { applyBackground(); }));
        for (int i = 0; i < generation->count(); ++i)
            if (auto *w = qobject_cast<QPushButton *>(generation->itemAt(i)->widget()))
                abilitySetupButtons << w;
        body->addLayout(generation);
        abilityBudgetStatus = new QLabel;
        abilityBudgetStatus->setWordWrap(true);
        body->addWidget(abilityBudgetStatus);
        abilityLockState = new QLabel;
        abilityLockState->setWordWrap(true);
        body->addWidget(abilityLockState);
        auto *progression = new QHBoxLayout;
        progression->addWidget(
            button("Save abilities & lock",
                   [this]
                   {
                       if (!lockAbilities())
                           displayError("Could not save abilities. Your setup remains open.");
                   }));
        progression->addWidget(button("Level up…", [this] { levelUp(); }));
        progression->addWidget(button("Pending level-up choices", [this] { showAdvancement(); }));
        body->addLayout(progression);
        auto *manual = new QLabel(
            "Enter base scores, including background increases. Feats & bonuses adds tracked "
            "effects "
            "to the total automatically; do not add those effects to the base scores again. "
            "Conditional actions and other features can be recorded in Features & notes.");
        manual->setWordWrap(true);
        manual->setObjectName("muted");
        body->addWidget(manual);
        body->addStretch();
        tabs->addTab(scroll(sheet), "Character");
        auto *skills = page(body);
        auto *skillsGrid = new QGridLayout;
        skillsGrid->addWidget(new QLabel("Skill"), 0, 0);
        skillsGrid->addWidget(new QLabel("Ability"), 0, 1);
        skillsGrid->addWidget(new QLabel("Training"), 0, 2);
        skillsGrid->addWidget(new QLabel("Bonus"), 0, 3);
        for (int i = 0; i < 18; ++i)
        {
            skillsGrid->addWidget(new QLabel(skillNames[i]), i + 1, 0);
            skillsGrid->addWidget(
                button(
                    "Details", [this, i]
                    { showRuleDescription(skillNames[i], basicChoiceDescription(skillNames[i])); }),
                i + 1, 5);
            skillsGrid->addWidget(new QLabel(abilities[skillAbilities[i]]), i + 1, 1);
            skillProf[i] = new QComboBox;
            skillProf[i]->addItems({"None", "Proficient", "Expertise"});
            skillProf[i]->setToolTip("Base training. Tracked feat, item, and ability effects are "
                                     "included in the total bonus separately.");
            skillsGrid->addWidget(skillProf[i], i + 1, 2);
            skillLabels[i] = new QLabel;
            skillsGrid->addWidget(skillLabels[i], i + 1, 3);
            skillsGrid->addWidget(
                button("Roll", [this, i] { d20(skillNames[i], skillBonus(i), skillTargets(i)); }),
                i + 1, 4);
            connect(skillProf[i], &QComboBox::currentIndexChanged, this,
                    [this, i](int training)
                    {
                        if (loading)
                            return;
                        QJsonArray a = array("skillProfs");
                        while (a.size() < 18)
                            a.append(0);
                        a[i] = training;
                        put("skillProfs", a);
                    });
        }
        body->addLayout(skillsGrid);
        body->addStretch();
        tabs->addTab(scroll(skills), "Skills");
        auto *combat = page(body);
        auto *combatForm = new QFormLayout;
        profLabel = new QLabel;
        initiativeLabel = new QLabel;
        passiveLabel = new QLabel;
        combatForm->addRow("Proficiency bonus", profLabel);
        combatForm->addRow("Initiative", initiativeLabel);
        combatForm->addRow("Extra initiative bonus", spin("initiativeBonus", -20, 30));
        combatForm->addRow("Passive Perception", passiveLabel);
        effectiveStats = new QLabel;
        effectiveStats->setWordWrap(true);
        body->addWidget(effectiveStats);
        combatForm->addRow("Base Armor Class", spin("ac", 0, 50));
        fields["ac"]->setToolTip("Normal armor plus base Dexterity, without bonuses from tracked "
                                 "sources. Effective AC is shown above.");
        auto *armor = new QCheckBox("Wearing Light, Medium, or Heavy armor");
        connect(armor, &QCheckBox::toggled, this,
                [this](bool value) { put("wearingArmor", value); });
        armorCheck = armor;
        combatForm->addRow(armor);
        combatForm->addRow("Dexterity changes affect AC",
                           choice("armorDexMode", {"Full", "Maximum +2", "None"}));
        combatForm->addRow("Base speed (ft)", spin("speed", 0, 300));
        combatForm->addRow("Base maximum HP", spin("hpMax", 1, 9999));
        combatForm->addRow("Current HP", spin("hpCurrent", 0, 9999));
        combatForm->addRow("Temporary HP", spin("hpTemp", 0, 9999));
        combatForm->addRow("Hit Dice spent", spin("hitDiceSpent", 0, 20));
        combatForm->addRow("Death save successes", spin("deathSuccess", 0, 3));
        combatForm->addRow("Death save failures", spin("deathFails", 0, 3));
        body->addLayout(combatForm);
        hpState = new QLabel;
        body->addWidget(hpState);
        auto *combatActions = new QHBoxLayout;
        combatActions->addWidget(button("Take damage", [this] { hpChange(false); }));
        combatActions->addWidget(button("Heal", [this] { hpChange(true); }));
        combatActions->addWidget(button("Roll death save", [this] { deathSave(); }));
        combatActions->addWidget(button("Concentration save",
                                        [this]
                                        {
                                            d20("Concentration save",
                                                saveBonus(2) + effectBonus({"concentration"}),
                                                {"save:*", "save:Constitution", "concentration"});
                                        }));
        combatActions->addWidget(button("Initiative",
                                        [this]
                                        {
                                            d20("Initiative", initiativeBonus(),
                                                {"initiative", "check:*", "check:Dexterity"});
                                        }));
        body->addLayout(combatActions);
        body->addWidget(button(
            "Set HP using class average",
            [this]
            {
                if (QMessageBox::question(
                        this, "Calculate HP",
                        "Set maximum and current HP using your class Hit Die, Constitution, and "
                        "average increases? This replaces manual HP values.") != QMessageBox::Yes)
                    return;
                int hp = averageHp();
                setNumber("hpMax", hp);
                setNumber("hpCurrent", maximumHp());
            }));
        body->addWidget(button(
            "Long rest",
            [this]
            {
                if (QMessageBox::question(
                        this, "Long rest",
                        "Restore HP, clear temporary HP and death saves, recover spent Hit Dice, "
                        "and reset spent spell slots and tracked class resources?") !=
                    QMessageBox::Yes)
                    return;
                setNumber("hpCurrent", maximumHp());
                for (const auto &key : {"hpTemp", "deathSuccess", "deathFails", "hitDiceSpent"})
                    setNumber(key, 0);
                QJsonArray used;
                for (int i = 0; i < 9; ++i)
                    used.append(0);
                put("slotsUsed", used);
                setNumber("pactUsed", 0);
                populateSlots();
                restoreClassResources(true);
                log("Long rest", "HP, Hit Dice, tracked spell slots and class resources restored.");
            }));
        body->addWidget(new QLabel("Conditions, exhaustion, resistances, armor choices, and "
                                   "special damage effects are tracked in notes."));
        body->addWidget(notes("conditions"), 1);
        tabs->addTab(scroll(combat), "Combat");
        auto *weapons = page(body);
        auto *weaponActions = new QHBoxLayout;
        weaponActions->addWidget(button("Add weapon", [this] { weaponDialog(); }));
        weaponActions->addWidget(button("Edit selected", [this] { weaponDialog(true); }));
        weaponActions->addWidget(button("Remove", [this] { removeEntry("weapons", weaponTable); }));
        weaponActions->addStretch();
        body->addLayout(weaponActions);
        attackActionCheck =
            new QCheckBox("Attack action on my turn (Heavy weapon feat damage applies only here)");
        attackActionCheck->setChecked(true);
        connect(attackActionCheck, &QCheckBox::toggled, this,
                [this](bool value) { put("attackAction", value); });
        body->addWidget(attackActionCheck);
        weaponTable = table({"Weapon", "Attack", "Damage dice", "Type", "Properties", "Mastery"});
        body->addWidget(weaponTable, 1);
        auto *attacks = new QHBoxLayout;
        attacks->addWidget(button("Roll attack", [this] { weaponRoll(false); }));
        attacks->addWidget(button("Roll damage", [this] { weaponRoll(true); }));
        criticalDamage = new QCheckBox("Critical damage (double dice)");
        attacks->addWidget(criticalDamage);
        attacks->addStretch();
        body->addLayout(attacks);
        auto *weaponNote =
            new QLabel("Mastery eligibility and class-feature effects are checked by you. "
                       "Versatile weapons can use their two-handed die in Edit selected. Damage "
                       "includes the selected ability and magic bonus.");
        weaponNote->setWordWrap(true);
        body->addWidget(weaponNote);
        tabs->addTab(weapons, "Weapons");
        auto *spells = page(body);
        legacyPdfSpellsCheck =
            new QCheckBox("Allow uploaded 2015 spell options (requires DM approval)");
        legacyPdfSpellsCheck->setObjectName("allowUploadedLegacySpells");
        connect(legacyPdfSpellsCheck, &QCheckBox::toggled, this,
                [this](bool checked)
                {
                    if (!loading)
                        put("allowLegacyPdfRules", checked);
                });
        body->addWidget(legacyPdfSpellsCheck);
        auto *casting = new QHBoxLayout;
        casting->addWidget(new QLabel("Spellcasting ability"));
        casting->addWidget(choice("spellAbility", abilities));
        spellLabel = new QLabel;
        casting->addWidget(spellLabel);
        casting->addStretch();
        body->addLayout(casting);
        auto *spellActions = new QHBoxLayout;
        spellActions->addWidget(button("Browse / add spell", [this] { addSpell(); }));
        spellActions->addWidget(button("Toggle prepared",
                                       [this]
                                       {
                                           int r = spellTable->currentRow();
                                           if (r < 0)
                                               return;
                                           auto a = array("spells");
                                           auto s = a[r].toObject();
                                           s["prepared"] = !s["prepared"].toBool();
                                           a[r] = s;
                                           put("spells", a);
                                           refreshSpells();
                                       }));
        spellActions->addWidget(button("Remove", [this] { removeEntry("spells", spellTable); }));
        spellActions->addWidget(button("Cast / roll effect", [this] { castSpell(); }));
        spellActions->addWidget(button("Set spell ability", [this] { setSpellAbility(); }));
        spellActions->addWidget(button(
            "Spell attack",
            [this]
            {
                int i = abilities.indexOf(selectedSpellAbility());
                d20("Spell attack", ability(i < 0 ? 3 : i) + pb() + effectBonus({"spellAttack"}),
                    {"spellAttack"});
            }));
        body->addLayout(spellActions);
        spellTable = table({"Spell", "Level", "Dice found in effect / scaling", "Prepared"});
        spellTable->setMaximumHeight(240);
        body->addWidget(spellTable, 1);
        spellDetails = new QPlainTextEdit;
        spellDetails->setReadOnly(true);
        spellDetails->setPlaceholderText(
            "Select a spell to read its casting details, effects, and scaling.");
        body->addWidget(spellDetails, 2);
        connect(spellTable, &QTableWidget::itemSelectionChanged, this,
                [this]
                {
                    int r = spellTable->currentRow();
                    spellDetails->setPlainText(
                        r < 0 ? QString()
                              : sourcesLast(findSpell(array("spells")
                                                          .at(r)
                                                          .toObject()["name"]
                                                          .toString())["description"]
                                                .toString()));
                });
        tabs->addTab(spells, "Spellbook");
        auto *slotsPage = page(body);
        body->addWidget(
            new QLabel("Track spell slots here. Maximums remain editable for feats, "
                       "subclasses, and multiclass characters. Pact Magic is tracked separately; "
                       "Mystic Arcanum remains in notes."));
        body->addWidget(button("Configure slots for class / level", [this] { configureSlots(); }));
        auto *pactForm = new QFormLayout;
        pactForm->addRow("Pact slot level", spin("pactLevel", 1, 5));
        pactForm->addRow("Pact slots maximum", spin("pactMax", 0, 10));
        pactForm->addRow("Pact slots spent", spin("pactUsed", 0, 10));
        body->addLayout(pactForm);
        body->addWidget(
            button("Short rest: recover Pact Magic", [this] { setNumber("pactUsed", 0); }));
        slotTable = table({"Slot level", "Maximum", "Spent"});
        body->addWidget(slotTable, 1);
        tabs->addTab(slotsPage, "Spell slots");
        featPage = page(body);
        auto *featIntro = new QLabel(
            "Select several feats and configure their choices. Base stats exclude tracked bonuses. "
            "Uncheck a source to disable its effects. Conditional actions, rerolls and resource "
            "uses are explained in each description.");
        featIntro->setWordWrap(true);
        body->addWidget(featIntro);
        auto *featActions = new QHBoxLayout;
        featActions->addWidget(button("Choose feats…", [this] { selectFeats(); }));
        featActions->addWidget(button("Custom feat", [this] { editOwnedFeat(true); }));
        featActions->addWidget(button("Edit effects / choices", [this] { editOwnedFeat(); }));
        featActions->addWidget(button("Remove feat",
                                      [this]
                                      {
                                          removeEntry("feats", featTable);
                                          refreshFeats();
                                      }));
        body->addLayout(featActions);
        featTable = table({"Active", "Feat", "Category", "Tracked effects"});
        featTable->setMinimumHeight(140);
        body->addWidget(featTable, 2);
        auto *featDetails = new QPlainTextEdit;
        featDetails->setReadOnly(true);
        featDetails->setPlaceholderText(
            "Select an owned feat to read its description and effects.");
        body->addWidget(featDetails, 2);
        connect(featTable, &QTableWidget::itemSelectionChanged, this,
                [this, featDetails]
                {
                    int r = featTable->currentRow();
                    if (r < 0)
                    {
                        featDetails->clear();
                        return;
                    }
                    auto f = array("feats").at(r).toObject();
                    featDetails->setPlainText(
                        sourcesLast(f["description"].toString() + "\n\n" + f["choices"].toString() +
                                    "\n\nAUTOMATION\n" + f["automation"].toString() +
                                    "\n\nTRACKED EFFECTS\n" + effectSummary(f)));
                });
        connect(featTable, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem *item)
                {
                    if (loading || item->column() != 0)
                        return;
                    auto feats = array("feats");
                    auto f = feats.at(item->row()).toObject();
                    f["active"] = item->checkState() == Qt::Checked;
                    feats[item->row()] = f;
                    put("feats", feats);
                });
        auto *sourceActions = new QHBoxLayout;
        sourceActions->addWidget(button("Add ability / other bonus", [this] { editSource(true); }));
        sourceActions->addWidget(button("Edit source", [this] { editSource(false); }));
        sourceActions->addWidget(button("Remove source",
                                        [this]
                                        {
                                            removeEntry("effectSources", sourceTable);
                                            refreshSources();
                                        }));
        body->addLayout(sourceActions);
        sourceTable = table({"Active", "Ability / other source", "Tracked effects"});
        body->addWidget(sourceTable, 1);
        connect(sourceTable, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem *item)
                {
                    if (loading || item->column() != 0)
                        return;
                    auto sources = array("effectSources");
                    auto f = sources.at(item->row()).toObject();
                    f["active"] = item->checkState() == Qt::Checked;
                    sources[item->row()] = f;
                    put("effectSources", sources);
                });
        effectsOverview = new QPlainTextEdit;
        effectsOverview->setReadOnly(true);
        effectsOverview->setMaximumHeight(110);
        body->addWidget(effectsOverview);
        tabs->addTab(featPage, "Feats & bonuses");
        auto *inventory = page(body);
        auto *coins = new QHBoxLayout;
        for (const QString &coin : {"cp", "sp", "ep", "gp", "pp"})
        {
            coins->addWidget(new QLabel(coin.toUpper()));
            coins->addWidget(spin(coin, 0, 999999));
        }
        body->addLayout(coins);
        auto *equipmentActions = new QHBoxLayout;
        equipmentActions->addWidget(button("Choose equipment…", [this] { selectEquipment(); }));
        equipmentActions->addWidget(button("Item details", [this] { showEquipmentDetails(); }));
        equipmentActions->addWidget(
            button("Add custom item",
                   [this]
                   {
                       auto a = array("inventory");
                       a.append(QJsonObject{
                           {"name", "New item"}, {"qty", "1"}, {"weight", "0"}, {"notes", ""}});
                       put("inventory", a);
                       populateInventory();
                   }));
        equipmentActions->addWidget(
            button("Remove selected", [this] { removeEntry("inventory", inventoryTable); }));
        equipmentActions->addWidget(button("Magic item preset", [this] { addMagicItem(); }));
        equipmentActions->addWidget(
            button("Magic items from PDFs…", [this] { browseUploadedRules("Magic items"); }));
        equipmentActions->addWidget(button("Item bonuses / equip", [this] { editItemEffects(); }));
        equipmentActions->addStretch();
        body->addLayout(equipmentActions);
        inventoryTable = table({"Item", "Quantity", "Weight per item (lb)", "Notes",
                                "Equipped / active", "Cost per item"},
                               true);
        inventoryTable->horizontalHeader()->setStretchLastSection(false);
        inventoryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        connect(inventoryTable, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem *item)
                {
                    if (loading)
                        return;
                    if (item->column() == 4 && item->checkState() == Qt::Checked)
                    {
                        auto chosen = array("inventory").at(item->row()).toObject();
                        QString category = chosen["armorCategory"].toString();
                        if (!category.isEmpty())
                        {
                            QSignalBlocker blocker(inventoryTable);
                            for (int r = 0; r < inventoryTable->rowCount(); ++r)
                            {
                                auto other = array("inventory").at(r).toObject();
                                QString type = other["armorCategory"].toString();
                                if (r != item->row() && !type.isEmpty() &&
                                    (type == "Shield") == (category == "Shield"))
                                    inventoryTable->item(r, 4)->setCheckState(Qt::Unchecked);
                            }
                        }
                    }
                    inventoryChanged();
                });
        body->addWidget(inventoryTable, 1);
        auto *carryingSettings = new QHBoxLayout;
        carryingSettings->addWidget(new QLabel("Size for carrying"));
        carryingSettings->addWidget(
            choice("carryingSize", {"Small / Medium", "Tiny", "Large", "Huge", "Gargantuan"}));
        carryingSettings->addWidget(new QLabel("Carrying trait"));
        carryingSettings->addWidget(
            choice("carryingTrait", {"Automatic", "Normal capacity", "Double capacity"}));
        auto *coinWeight = new QCheckBox("Include coin weight");
        coinWeight->setObjectName("includeCoinWeight");
        fields["includeCoinWeight"] = coinWeight;
        connect(coinWeight, &QCheckBox::toggled, this,
                [this](bool enabled) { put("includeCoinWeight", enabled); });
        carryingSettings->addWidget(coinWeight);
        carryingSettings->addStretch();
        body->addLayout(carryingSettings);
        weightLabel = new QLabel;
        weightLabel->setObjectName("carryingCapacitySummary");
        weightLabel->setWordWrap(true);
        body->addWidget(weightLabel);
        carryingProgress = new QProgressBar;
        carryingProgress->setRange(0, 100);
        body->addWidget(carryingProgress);
        auto *carryingHint =
            new QLabel("All inventory rows count, including worn gear. Coins weigh 1 lb per 50 "
                       "coins. Automatic carrying traits support enabled Goliath, Bugbear and "
                       "Firbolg profiles. Adjust size for temporary forms; adjust item weight for "
                       "magical storage. Over the maximum, reduce the load or move it separately.");
        carryingHint->setWordWrap(true);
        body->addWidget(carryingHint);
        tabs->addTab(inventory, "Inventory");
        auto *journal = page(body);
        body->addWidget(new QLabel("Class features, species traits, feats, proficiencies, "
                                   "resources, and spell exceptions"));
        body->addWidget(notes("features"), 1);
        body->addWidget(new QLabel("Appearance, personality, backstory, and campaign notes"));
        body->addWidget(notes("journal"), 1);
        tabs->addTab(journal, "Features and notes");
        auto *dice = page(body);
        buildDiceTray(body);
        body->addWidget(new QLabel("Quick rolls"));
        auto *quick = new QHBoxLayout;
        for (int sides : {4, 6, 8, 10, 12, 20, 100})
            quick->addWidget(button("d" + QString::number(sides),
                                    [this, sides]
                                    {
                                        if (sides == 20)
                                        {
                                            d20("d20", 0);
                                            return;
                                        }
                                        auto r = rollDice("1d" + QString::number(sides));
                                        log("d" + QString::number(sides),
                                            r.detail + " = " + QString::number(r.total));
                                    }));
        body->addLayout(quick);
        rollHistory = new QPlainTextEdit;
        rollHistory->setReadOnly(true);
        body->addWidget(rollHistory, 1);
        tabs->addTab(dice, "Dice journal");
        buildAdvancementUi();
        buildSubclassFeatureUi();
        auto *file = menuBar()->addMenu("Characters");
        auto *saveAction = file->addAction("Save");
        saveAction->setShortcut(QKeySequence::Save);
        connect(saveAction, &QAction::triggered, this,
                [this]
                {
                    if (!lockAbilities())
                        displayError(
                            "Could not save. Export a character backup to protect your changes.");
                });
        connect(file->addAction("Open character folder"), &QAction::triggered, this,
                [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(directory)); });
        connect(file->addAction("Delete current character…"), &QAction::triggered, this,
                [this]
                {
                    if (QMessageBox::question(this, "Delete character",
                                              "Move this character to the Trash or Recycle Bin?") !=
                        QMessageBox::Yes)
                        return;
                    if (!save() || !QFile::moveToTrash(path()))
                    {
                        displayError("Could not delete this character.");
                        return;
                    }
                    current.clear();
                    refreshLibrary();
                    if (characters->count())
                    {
                        characters->setCurrentRow(0);
                        openCharacter(characters->item(0)->data(Qt::UserRole).toString());
                    }
                    else
                        newCharacter();
                });
        auto *view = menuBar()->addMenu("Appearance");
        auto *darkMode = view->addAction("Dark mode");
        darkMode->setCheckable(true);
        darkMode->setChecked(QSettings().value("appearance/dark", false).toBool());
        connect(darkMode, &QAction::toggled, this, [this](bool enabled) {
            QSettings().setValue("appearance/dark", enabled);
            LedgerTheme::apply(enabled);
            updateWeight();
        });
        auto *help = menuBar()->addMenu("Rules and help");
        connect(help->addAction("Check for updates…"), &QAction::triggered, this,
                [this] { LedgerUpdater::check(this, [this] { return save(); }, directory); });
        connect(help->addAction("Sources and version notes…"), &QAction::triggered, this,
                [this] { browseAllSources(); });
        connect(help->addAction("Uploaded PDFs and older-rule references…"), &QAction::triggered,
                this, [this] { browseUploadedRules(); });
        connect(help->addAction("Open offline 2024 rules (SRD)"), &QAction::triggered, this,
                [this]
                {
                    QString reference = QCoreApplication::applicationDirPath() + "/SRD-5.2.1.pdf";
                    if (!QFile::exists(reference))
                    {
                        displayError("The offline SRD reference is missing from the app folder.");
                        return;
                    }
                    QDesktopServices::openUrl(QUrl::fromLocalFile(reference));
                });
        connect(
            help->addAction("About / rules attribution"), &QAction::triggered, this,
            [this]
            {
                QMessageBox::information(
                    this, "Adventurer’s Ledger",
                    "A private, offline character sheet compatible with fifth edition’s 2024 "
                    "rules.\n\n" +
                        catalog["attribution"].toString() +
                        "\n\nThis is an independent app. Character building is editable: class and "
                        "background skill choices, features, feats, multiclassing, and special "
                        "spell effects need your review. Dice shown in spell references include "
                        "alternative/scaling effects; confirm the correct roll when casting.");
            });
        connect(cls, &QComboBox::currentTextChanged, this, [this] { classDefaults(); });
        connect(qobject_cast<QComboBox *>(fields["species"]), &QComboBox::currentTextChanged, this,
                [this](const QString &s)
                {
                    if (!loading)
                    {
                        Q_UNUSED(s);
                        put("speciesProfile", QJsonObject{});
                    }
                });
        connect(search, &QLineEdit::textChanged, this,
                [this]
                {
                    if (save())
                        refreshLibrary();
                });
        connect(characters, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem *item)
                {
                    if (item)
                        openCharacter(item->data(Qt::UserRole).toString());
                });
    }

  protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (!save())
        {
            displayError("Could not save your character. Export a backup before closing.");
            event->ignore();
            return;
        }
        QSettings().setValue("geometry", saveGeometry());
        event->accept();
    }

  public:
    Ledger(const QString &overrideDirectory = {})
        : weightLabel(nullptr), weaponTable(nullptr), spellTable(nullptr)
    {
        QFile file(":/catalog.json");
        if (file.open(QIODevice::ReadOnly))
            catalog = QJsonDocument::fromJson(file.readAll()).object();
        auto combinedSpells = catalog["spells"].toArray();
        for (auto value : uploadedRuleCatalog()["spells"].toArray())
            combinedSpells.append(value);
        catalog["spells"] = combinedSpells;
        directory = overrideDirectory.isEmpty()
                        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                              "/characters"
                        : overrideDirectory;
        if (!QDir().mkpath(directory))
            throw std::runtime_error("Could not create the local character folder.");
        setWindowTitle("Adventurer’s Ledger — 2024 character sheets");
        resize(1370, 900);
        setMinimumSize(1100, 720);
        buildUi();
        autosave.setSingleShot(true);
        connect(&autosave, &QTimer::timeout, this, [this] { save(); });
        restoreGeometry(QSettings().value("geometry").toByteArray());
        refreshLibrary();
        if (characters->count())
            characters->setCurrentRow(0);
        else
            newCharacter();
    }
    bool selfTest()
    {
        auto mainTestFailure = [&](int line)
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (!file.exists() && file.open(QIODevice::WriteOnly))
                file.write(("Main self-test line " + QString::number(line)).toUtf8());
            return false;
        };
        character["abilityBudgetManaged"] = false;
        loadWidgets();
        if (modifier(9) != -1 || modifier(8) != -1 || modifier(7) != -2 || proficiency(5) != 3 ||
            proficiency(20) != 6)
            return mainTestFailure(__LINE__);
        for (int i = 0; i < 100; ++i)
        {
            auto r = rollDice("2d6+3");
            if (r.total < 5 || r.total > 15)
                return mainTestFailure(__LINE__);
            auto crit = rollDice("1d6+3", true);
            if (crit.total < 5 || crit.total > 15)
                return mainTestFailure(__LINE__);
        }
        bool rejected = false;
        try
        {
            rollDice("hello");
        }
        catch (...)
        {
            rejected = true;
        }
        if (!rejected)
            return mainTestFailure(__LINE__);
        if (catalog["spells"].toArray().size() !=
                339 + uploadedRuleCatalog()["spells"].toArray().size() ||
            catalog["weapons"].toArray().size() != 38)
            return mainTestFailure(__LINE__);
        qobject_cast<QLineEdit *>(fields["name"])->setText("Éowyn — test adventurer");
        setNumber("Strength", 18);
        setNumber("Dexterity", 9);
        setNumber("level", 5);
        setNumber("hpMax", 35);
        setNumber("hpCurrent", 11);
        setNumber("hpTemp", 7);
        put("weapons", QJsonArray{QJsonObject{{"name", "Longsword"},
                                              {"dice", "1d8"},
                                              {"properties", "Versatile (1d10)"},
                                              {"type", "Slashing"},
                                              {"mastery", "Sap"},
                                              {"ability", 0},
                                              {"trained", true},
                                              {"twoHanded", true}}});
        put("spells", QJsonArray{QJsonObject{{"name", "Fireball"}, {"prepared", true}}});
        put("inventory", QJsonArray{QJsonObject{
                             {"name", "Rope"}, {"qty", "2"}, {"weight", "5"}, {"notes", "50 ft"}}});
        if (spellFormula("Fire Bolt", 0) != "2d10" || spellFormula("Cure Wounds", 2) != "4d8+1")
            return mainTestFailure(__LINE__);
        if (weaponDice(array("weapons").first().toObject()) != "1d10")
            return mainTestFailure(__LINE__);
        if (!save())
            return mainTestFailure(__LINE__);
        QString original = current;
        newCharacter();
        character["abilityBudgetManaged"] = false;
        loadWidgets();
        if (!array("weapons").isEmpty())
            return mainTestFailure(__LINE__);
        openCharacter(original);
        if (text("name") != "Éowyn — test adventurer" || number("hpCurrent") != 11 ||
            number("hpTemp") != 7 || array("weapons").size() != 1 || array("spells").size() != 1 ||
            array("inventory").size() != 1)
            return mainTestFailure(__LINE__);
        if (profLabel->text() != "+3" || modLabels[1]->text() != "9 (-1)")
            return mainTestFailure(__LINE__);
        applyHpChange(5, false);
        if (number("hpTemp") != 2 || number("hpCurrent") != 11)
            return mainTestFailure(__LINE__);
        applyHpChange(6, false);
        if (number("hpTemp") != 0 || number("hpCurrent") != 7)
            return mainTestFailure(__LINE__);
        setNumber("deathFails", 2);
        applyHpChange(100, true);
        if (number("hpCurrent") != 35 || number("deathFails") != 0)
            return mainTestFailure(__LINE__);
        setNumber("hpCurrent", 0);
        applyDeathSave(1);
        if (number("deathFails") != 2)
            return mainTestFailure(__LINE__);
        applyDeathSave(20);
        if (number("hpCurrent") != 1 || number("deathFails") != 0)
            return mainTestFailure(__LINE__);
        auto *skill = skillProf[3];
        skill->setCurrentIndex(2);
        if (skillBonus(3) != 10)
            return mainTestFailure(__LINE__);
        const QString testId = current;
        setNumber("hpCurrent", 25);
        save();
        newCharacter();
        character["abilityBudgetManaged"] = false;
        loadWidgets();
        openCharacter(testId);
        if (number("hpCurrent") != 25)
            return mainTestFailure(__LINE__);
        // Verify multiclass calculations and saved selections in the temporary test library.
        character["class"] = "Fighter";
        character["primaryLevel"] = 3;
        character["level"] = 5;
        character["multiclasses"] = QJsonArray{QJsonObject{{"name", "Wizard"}, {"level", 2}}};
        if (pb() != 3 || averageHp() != 35 || spellFormula("Fire Bolt", 0) != "2d10")
            return mainTestFailure(__LINE__);
        configureSlots(false);
        if (array("slotsMax").at(0).toInt() != 3 || number("pactMax") != 0)
            return mainTestFailure(__LINE__);
        character["class"] = "Paladin";
        character["primaryLevel"] = 3;
        configureSlots(false);
        if (array("slotsMax").at(0).toInt() != 4 || array("slotsMax").at(1).toInt() != 3)
            return mainTestFailure(__LINE__);
        character["class"] = "Warlock";
        character["primaryLevel"] = 3;
        configureSlots(false);
        if (array("slotsMax").at(0).toInt() != 3 || array("slotsMax").at(1).toInt() != 0 ||
            number("pactMax") != 2 || number("pactLevel") != 2)
            return mainTestFailure(__LINE__);
        character["class"] = "Fighter";
        character["subclass"] = "Eldritch Knight";
        character["primaryLevel"] = 6;
        character["level"] = 8;
        configureSlots(false);
        if (array("slotsMax").at(1).toInt() != 3)
            return mainTestFailure(__LINE__);
        character["class"] = "Ranger";
        character["primaryLevel"] = 4;
        character["level"] = 7;
        character["multiclasses"] = QJsonArray{QJsonObject{{"name", "Sorcerer"}, {"level", 3}}};
        configureSlots(false);
        if (array("slotsMax").at(2).toInt() != 2)
            return mainTestFailure(__LINE__);
        qobject_cast<QLineEdit *>(fields["languages"])->setText("Common, Elvish, Draconic");
        save();
        QString multiId = current;
        newCharacter();
        character["abilityBudgetManaged"] = false;
        loadWidgets();
        openCharacter(multiId);
        if (number("primaryLevel") != 4 || number("level") != 7 ||
            array("multiclasses").size() != 1 || text("languages") != "Common, Elvish, Draconic")
            return mainTestFailure(__LINE__);
        bool rosterAccepted = false;
        for (const auto &cls : classes)
        {
            QComboBox test;
            populateSubclasses(&test, cls);
            if (test.count() != subclasses.value(cls).size() + 1 ||
                subclasses.value(cls).size() < 4)
                return mainTestFailure(__LINE__);
        }
        auto *sheetSubclass = qobject_cast<QComboBox *>(fields["subclass"]);
        if (!sheetSubclass || sheetSubclass->findText("Hunter") < 0)
            return mainTestFailure(__LINE__);
        sheetSubclass->setCurrentText("Hunter");
        if (text("subclass") != "Hunter")
            return mainTestFailure(__LINE__);
        QTimer::singleShot(
            0, this,
            [&]
            {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto *roster = dialog->findChild<QTableWidget *>();
                if (!roster || roster->rowCount() != 2)
                {
                    dialog->reject();
                    return;
                }
                qobject_cast<QSpinBox *>(roster->cellWidget(0, 1))->setValue(5);
                auto *firstSubclass = qobject_cast<QComboBox *>(roster->cellWidget(0, 2));
                auto *secondSubclass = qobject_cast<QComboBox *>(roster->cellWidget(1, 2));
                if (!firstSubclass || !secondSubclass || firstSubclass->currentText() != "Hunter" ||
                    secondSubclass->findText("Wild Magic Sorcery") < 0)
                {
                    dialog->reject();
                    return;
                }
                qobject_cast<QComboBox *>(roster->cellWidget(0, 0))->setCurrentText("Fighter");
                if (firstSubclass->findText("Hunter") >= 0 ||
                    firstSubclass->findText("Eldritch Knight") < 0 ||
                    !firstSubclass->currentText().isEmpty())
                {
                    dialog->reject();
                    return;
                }
                firstSubclass->setCurrentText("Eldritch Knight");
                secondSubclass->setCurrentText("Wild Magic Sorcery");
                dialog->grab().save(QCoreApplication::applicationDirPath() +
                                    "/preview-classes.png");
                rosterAccepted = true;
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            });
        editClasses();
        if (!rosterAccepted || number("primaryLevel") != 5 || number("level") != 8 || pb() != 3)
            return mainTestFailure(__LINE__);
        if (text("subclass") != "Eldritch Knight" ||
            array("multiclasses").first().toObject()["subclass"].toString() != "Wild Magic Sorcery")
            return mainTestFailure(__LINE__);
        save();
        QString subclassesId = current;
        newCharacter();
        character["abilityBudgetManaged"] = false;
        loadWidgets();
        openCharacter(subclassesId);
        if (sheetSubclass->currentText() != "Eldritch Knight" ||
            sheetSubclass->findText("Hunter") >= 0 ||
            array("multiclasses").first().toObject()["subclass"].toString() != "Wild Magic Sorcery")
            return mainTestFailure(__LINE__);
        // Older/custom subclass names survive switching between saved characters.
        character["subclass"] = "Custom tradition";
        loadWidgets();
        if (sheetSubclass->currentText() != "Custom tradition")
            return mainTestFailure(__LINE__);
        character["subclass"] = "Eldritch Knight";
        loadWidgets();
        if (sheetSubclass->findText("Custom tradition") >= 0)
            return mainTestFailure(__LINE__);
        bool languagesAccepted = false;
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
                {
                    auto *item = list->item(i);
                    item->setCheckState(
                        QStringList{"Common", "Elvish", "Sylvan"}.contains(item->text())
                            ? Qt::Checked
                            : Qt::Unchecked);
                }
                dialog->grab().save(QCoreApplication::applicationDirPath() +
                                    "/preview-languages.png");
                languagesAccepted = true;
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
        chooseLanguages();
        if (!languagesAccepted || text("languages") != "Common, Elvish, Sylvan")
            return mainTestFailure(__LINE__);
        if (!selfTestEncumbrance())
            return false;
        if (!selfTestDeities())
            return false;
        if (!selfTestUploadedRules())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestUploadedRules");
            return false;
        }
        if (!selfTestSkillTrainingDisplay())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestSkillTrainingDisplay");
            return false;
        }
        if (!selfTestBookCatalogs())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestBookCatalogs");
            return false;
        }
        if (!selfTestFullFeats())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestFullFeats");
            return false;
        }
        if (!selfTestEffects())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestEffects");
            return false;
        }
        if (!selfTestProgression())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestProgression");
            return false;
        }
        if (!selfTestEquipment())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestEquipment");
            return false;
        }
        if (!selfTestAdvancement())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestAdvancement");
            return false;
        }
        if (!selfTestCreation())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestCreation");
            return false;
        }
        if (!selfTestLevelChoices())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestLevelChoices");
            return false;
        }
        if (!selfTestAbilityBudgets())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestAbilityBudgets");
            return false;
        }
        if (!selfTestExpandedSubclasses())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestExpandedSubclasses");
            return false;
        }
        if (!selfTestSubclassFeatureEngine())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestSubclassFeatureEngine");
            return false;
        }
        if (!selfTestIncrementalChoices())
        {
            QFile file(QCoreApplication::applicationDirPath() + "/self-test-failure.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write("\nselfTestIncrementalChoices");
            return false;
        }
        auto *topRoll = findChild<QPushButton *>("topDiceRoll");
        if (!topRoll || diceInput->count() != 7)
            return mainTestFailure(__LINE__);
        for (int i = 0; i < diceInput->count(); ++i)
        {
            diceInput->setCurrentIndex(i);
            int sides = diceInput->currentText().mid(2).toInt();
            auto before = array("rollLog").size();
            topRoll->click();
            if (!diceVisual->rolling() || array("rollLog").size() != before + 1)
                return mainTestFailure(__LINE__);
            topRoll->click();
            if (array("rollLog").size() != before + 1)
                return mainTestFailure(__LINE__);
            QEventLoop settle;
            if (i == 5)
                QTimer::singleShot(350, this,
                                   [this]
                                   {
                                       grab().save(QCoreApplication::applicationDirPath() +
                                                   "/preview-dice-rolling.png");
                                   });
            QTimer::singleShot(1100, &settle, &QEventLoop::quit);
            settle.exec();
            if (diceVisual->rolling() || diceVisual->displayedFace() < 1 ||
                diceVisual->displayedFace() > sides ||
                diceVisual->displayedFace() != diceVisual->displayedTotal())
                return mainTestFailure(__LINE__);
        }
        diceInput->setCurrentText("1d20+5");
        rollMode->setCurrentIndex(1);
        topRoll->click();
        {
            QEventLoop settle;
            QTimer::singleShot(1100, &settle, &QEventLoop::quit);
            settle.exec();
        }
        if (diceVisual->displayedTotal() != diceVisual->displayedFace() + 5 ||
            !array("rollLog").last().toString().contains("advantage"))
            return mainTestFailure(__LINE__);
        diceInput->setCurrentText("2d6+3");
        rollMode->setCurrentIndex(0);
        topRoll->click();
        {
            QEventLoop settle;
            QTimer::singleShot(1100, &settle, &QEventLoop::quit);
            settle.exec();
        }
        if (diceVisual->displayedTotal() < 5 || diceVisual->displayedTotal() > 15 ||
            diceVisual->displayedFace() > 6)
            return mainTestFailure(__LINE__);
        diceInput->setCurrentText("1d20");
        topRoll->click();
        {
            QEventLoop settle;
            QTimer::singleShot(1100, &settle, &QEventLoop::quit);
            settle.exec();
        }
        for (int i = 0; i < tabs->count(); ++i)
        {
            tabs->setCurrentIndex(i);
            QApplication::processEvents();
            grab().save(QCoreApplication::applicationDirPath() +
                        QString("/preview-tab-%1.png").arg(i));
        }
        if (!selfTestDiceTray())
            return mainTestFailure(__LINE__);
        tabs->setCurrentIndex(0);
        return save();
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("AdventurersLedger");
    app.setApplicationName("AdventurersLedger");
    app.setApplicationVersion(LedgerUpdater::version());
    app.setApplicationDisplayName("Adventurer’s Ledger");
    app.setWindowIcon(ledgerIcon());
    app.setStyle("Fusion");
#ifdef Q_OS_MACOS
    app.setFont(QFont("Palatino", 13));
#else
    app.setFont(QFont("Palatino Linotype", 10));
#endif
    if (app.arguments().contains("--dark-mode")) QSettings().setValue("appearance/dark", true);
    LedgerTheme::apply(QSettings().value("appearance/dark", false).toBool());
    try
    {
        QTemporaryDir temporary;
        bool test = app.arguments().contains("--self-test");
        Ledger window(test ? temporary.path() : QString());
        window.show();
        if (test)
            QTimer::singleShot(700, &app,
                               [&]
                               {
                                   bool okay = LedgerUpdater::selfTest() && window.selfTest();
                                   window.grab().save(QCoreApplication::applicationDirPath() +
                                                      "/preview.png");
                                   app.windowIcon().pixmap(256).save(
                                       QCoreApplication::applicationDirPath() + "/icon.png");
                                   LedgerTheme::apply(true);
                                   okay = okay && qApp->palette().color(QPalette::Window).lightness() < 80;
                                   window.grab().save(QCoreApplication::applicationDirPath() + "/preview-dark.png");
                                   LedgerTheme::apply(false);
                                   okay = okay && qApp->palette().color(QPalette::Window).lightness() > 180;
                                   app.exit(okay ? 0 : 2);
                               });
        return app.exec();
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(nullptr, "Adventurer’s Ledger", QString::fromUtf8(e.what()));
        return 1;
    }
}

