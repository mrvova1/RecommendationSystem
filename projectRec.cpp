#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

using namespace std;

namespace RecSys {

    // Структура для тега: имя и значение.
    struct Tag {
        string name;
        double value;
    };

    // Структура для произведения (work):
    // id — уникальный идентификатор,
    // tags — список тегов,
    // viewCount — число просмотров,
    // interactionTime — среднее время взаимодействия (например, в секундах).
    struct Work {
        string id;
        vector<Tag> tags;
        double viewCount;
        double interactionTime;
    };

    // Структура для профиля пользователя: набор тегов.
    struct UserProfile {
        vector<Tag> tags;
    };

    // Структура для похожего пользователя (для коллаборативной фильтрации).
    // similarity – коэффициент схожести с текущим пользователем,
    // likedWorks – список идентификаторов понравившихся произведений.
    struct SimilarUser {
        string id;
        double similarity;
        vector<string> likedWorks;
    };

    // Структура для дополнительных параметров, определяющих влияние метрик.
    struct MetricsConfig {
        bool useMetrics;         // использовать ли дополнительные метрики (просмотры и время)
        double weightViews;      // вес просмотров (ожидается значение в диапазоне 0..1)
        double weightTime;       // вес времени взаимодействия (0..1)
        double weightTags;       // вес оценки на основе тегов (например, 1.0 для полной важности)
    };

    // --- Функции для вычисления оценок рекомендаций ---

    // Функция вычисления косинусного сходства между профилем пользователя и произведением по тегам.
    // Вход: профиль пользователя и произведение.
    // Выход: значение сходства (от 0 до 1).
    double cosineSimilarity(const UserProfile &user, const Work &work) {
        double dot = 0.0;
        double normUser = 0.0;
        double normWork = 0.0;

        for (const auto &tag : user.tags) {
            normUser += tag.value * tag.value;
        }
        for (const auto &tag : work.tags) {
            normWork += tag.value * tag.value;
            for (const auto &utag : user.tags) {
                if (utag.name == tag.name) {
                    dot += utag.value * tag.value;
                    break;
                }
            }
        }
        normUser = sqrt(normUser);
        normWork = sqrt(normWork);
        if (normUser == 0 || normWork == 0) return 0;
        return dot / (normUser * normWork);
    }

    // Функция для расчёта "контент‑оценки" работы с учётом тегов и дополнительных метрик.
    // Для метрик производится нормализация по максимальным значениям среди всех работ.
    // Параметры влияния задаются через config.
    double computeWorkScore(const UserProfile &user, const Work &work, const MetricsConfig &config,
                            double maxViews, double maxTime) {
        double score = config.weightTags * cosineSimilarity(user, work);
        if (config.useMetrics) {
            double normViews = (maxViews > 0) ? work.viewCount / maxViews : 0;
            double normTime = (maxTime > 0) ? work.interactionTime / maxTime : 0;
            score += config.weightViews * normViews + config.weightTime * normTime;
        }
        return score;
    }

    // Функция контент‑бейзед рекомендаций.
    // Вход:
    //   - user: профиль пользователя.
    //   - works: список произведений.
    //   - config: параметры влияния метрик.
    // Выход: вектор пар (идентификатор работы, итоговая оценка), отсортированный по убыванию.
    vector<pair<string, double>> recommendContentBased(const UserProfile &user,
                                                        const vector<Work>& works,
                                                        const MetricsConfig &config) {
        // Вычисляем максимальные значения для нормализации метрик.
        double maxViews = 0, maxTime = 0;
        for (const auto &work : works) {
            if (work.viewCount > maxViews) maxViews = work.viewCount;
            if (work.interactionTime > maxTime) maxTime = work.interactionTime;
        }
        vector<pair<string, double>> recs;
        for (const auto &work : works) {
            double score = computeWorkScore(user, work, config, maxViews, maxTime);
            recs.push_back({work.id, score});
        }
        sort(recs.begin(), recs.end(), [](auto &a, auto &b) {
            return a.second > b.second;
        });
        return recs;
    }

    // Функция коллаборативной фильтрации.
    // Вход: список похожих пользователей (каждый с коэффициентом сходства и списком понравившихся работ).
    // Выход: вектор пар (идентификатор работы, агрегированная оценка), где оценка – сумма весов.
    vector<pair<string, double>> recommendCollaborative(const vector<SimilarUser>& similarUsers) {
        unordered_map<string, double> scoreMap;
        for (const auto &user : similarUsers) {
            for (const auto &workId : user.likedWorks) {
                scoreMap[workId] += user.similarity;
            }
        }
        vector<pair<string, double>> recs(scoreMap.begin(), scoreMap.end());
        sort(recs.begin(), recs.end(), [](auto &a, auto &b) {
            return a.second > b.second;
        });
        return recs;
    }

    // Функция объединения рекомендаций двух методов.
    // Вход:
    //   - contentRecs: рекомендации на основе контента.
    //   - collabRecs: рекомендации на основе коллаборативной фильтрации.
    //   - contentWeight, collabWeight: коэффициенты важности каждого метода.
    // Выход: объединённый отсортированный список (идентификатор работы, итоговая оценка).
    vector<pair<string, double>> combineRecommendations(
        const vector<pair<string, double>> &contentRecs,
        const vector<pair<string, double>> &collabRecs,
        double contentWeight = 0.5,
        double collabWeight = 0.5)
    {
        unordered_map<string, double> combined;
        for (const auto &p : contentRecs) {
            combined[p.first] += contentWeight * p.second;
        }
        for (const auto &p : collabRecs) {
            combined[p.first] += collabWeight * p.second;
        }
        vector<pair<string, double>> recs(combined.begin(), combined.end());
        sort(recs.begin(), recs.end(), [](auto &a, auto &b) {
            return a.second > b.second;
        });
        return recs;
    }

    // Функция для рандомизации итогового списка рекомендаций.
    // При каждом обновлении выдача немного перемешивается, чтобы пользователь видел разнообразие.
    // Вход:
    //   - recs: исходный отсортированный список рекомендаций;
    //   - numRecommendations: требуемое число рекомендаций;
    //   - randomFactor: доля случайных рекомендаций (например, 0.2 означает 20% случайных).
    // Выход: итоговый вектор пар (идентификатор работы, оценка).
    vector<pair<string, double>> getRandomizedRecommendations(
        const vector<pair<string, double>> &recs,
        int numRecommendations,
        double randomFactor)
    {
        if (numRecommendations <= 0) return {};
        int numRandom = static_cast<int>(numRecommendations * randomFactor);
        int numTop = numRecommendations - numRandom;
        vector<pair<string, double>> finalRecs;

        // Добавляем топовые рекомендации.
        for (size_t i = 0; i < static_cast<size_t>(numTop) && i < recs.size(); i++) {
            finalRecs.push_back(recs[i]);
        }
        // Остальные рекомендации для случайного выбора.
        vector<pair<string, double>> remaining;
        for (int i = 0; i < numTop && i < static_cast<int>(recs.size()); i++) {
            remaining.push_back(recs[i]);
        }
        random_device rd;
        mt19937 g(rd());
        shuffle(remaining.begin(), remaining.end(), g);
        for (int i = 0; i < numRandom && i < static_cast<int>(remaining.size()); i++) {
            finalRecs.push_back(remaining[i]);
        }
        shuffle(finalRecs.begin(), finalRecs.end(), g);
        return finalRecs;
    }

} // namespace RecSys

// --- Вспомогательная функция для удаления пробелов в начале и конце строки.
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

//
// --- Основной блок чтения входных данных ---
//
// Формат входных данных (через стандартный ввод):
//
// USER_PROFILE
// <число тегов пользователя>
// Для каждого тега: <имя_тега> <значение>
//
// WORKS
// <число произведений>
// Для каждого произведения:
//   <идентификатор работы>
//   <число тегов>
//   Для каждого тега: <имя_тега> <значение>
//   <viewCount> <interactionTime>   (метрики – число просмотров и время взаимодействия)
//
// SIMILAR_USERS
// <число похожих пользователей>
// Для каждого похожего пользователя:
//   <идентификатор пользователя>
//   <коэффициент сходства>
//   <число понравившихся работ>
//   Для каждого: <идентификатор работы>
//
// PARAMS
// <число рекомендаций> <random_factor>
// METRICS_CONFIG
// <use_metrics(0/1)> <weight_views> <weight_time> <weight_tags>
//

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;

    // Чтение секции USER_PROFILE
    while(getline(cin, line)) {
        if(trim(line) == "USER_PROFILE") break;
    }
    int numUserTags;
    cin >> numUserTags;
    vector<RecSys::Tag> userTags;
    for (int i = 0; i < numUserTags; i++) {
        string tagName;
        double tagValue;
        cin >> tagName >> tagValue;
        userTags.push_back({tagName, tagValue});
    }
    RecSys::UserProfile userProfile { userTags };

    // Чтение секции WORKS
    while(getline(cin, line)) {
        if(trim(line) == "WORKS") break;
    }
    int numWorks;
    cin >> numWorks;
    vector<RecSys::Work> works;
    for (int i = 0; i < numWorks; i++) {
        string workId;
        cin >> workId;
        int numTags;
        cin >> numTags;
        vector<RecSys::Tag> tags;
        for (int j = 0; j < numTags; j++) {
            string tagName;
            double tagValue;
            cin >> tagName >> tagValue;
            tags.push_back({tagName, tagValue});
        }
        double viewCount, interactionTime;
        cin >> viewCount >> interactionTime;
        works.push_back({workId, tags, viewCount, interactionTime});
    }

    // Чтение секции SIMILAR_USERS
    while(getline(cin, line)) {
        if(trim(line) == "SIMILAR_USERS") break;
    }
    int numSimilarUsers;
    cin >> numSimilarUsers;
    vector<RecSys::SimilarUser> similarUsers;
    for (int i = 0; i < numSimilarUsers; i++) {
        string simUserId;
        cin >> simUserId;
        double simWeight;
        cin >> simWeight;
        int numLiked;
        cin >> numLiked;
        vector<string> likedWorks;
        for (int j = 0; j < numLiked; j++) {
            string workId;
            cin >> workId;
            likedWorks.push_back(workId);
        }
        similarUsers.push_back({simUserId, simWeight, likedWorks});
    }

    // Чтение секции PARAMS
    while(getline(cin, line)) {
        if(trim(line) == "PARAMS") break;
    }
    int numRecommendations;
    double randomFactor;
    cin >> numRecommendations >> randomFactor;

    // Чтение секции METRICS_CONFIG
    while(getline(cin, line)) {
        if(trim(line) == "METRICS_CONFIG") break;
    }
    int useMetricsInt;
    double weightViews, weightTime, weightTags;
    cin >> useMetricsInt >> weightViews >> weightTime >> weightTags;
    RecSys::MetricsConfig metricsConfig { useMetricsInt != 0, weightViews, weightTime, weightTags };

    // Получение рекомендаций:
    // 1. Контент‑бейзед (с учетом тегов и метрик).
    auto contentRecs = RecSys::recommendContentBased(userProfile, works, metricsConfig);
    // 2. Коллаборативная фильтрация.
    auto collabRecs = RecSys::recommendCollaborative(similarUsers);
    // 3. Объединение рекомендаций (коэффициенты задаются равномерно для примера).
    auto combinedRecs = RecSys::combineRecommendations(contentRecs, collabRecs, 0.5, 0.5);
    // 4. Рандомизация итогового списка.
    auto finalRecs = RecSys::getRandomizedRecommendations(combinedRecs, numRecommendations, randomFactor);

    // Вывод результата в формате JSON.
    cout << "{\n  \"recommendations\": [\n";
    for (size_t i = 0; i < finalRecs.size(); i++) {
        cout << "    { \"id\": \"" << finalRecs[i].first << "\", \"score\": " << finalRecs[i].second << " }";
        if(i < finalRecs.size() - 1) cout << ",";
        cout << "\n";
    }
    cout << "  ]\n}\n";

    return 0;
}
