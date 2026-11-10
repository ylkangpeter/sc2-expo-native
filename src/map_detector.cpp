#include "map_detector.h"
#include <algorithm>

MapDetector& MapDetector::Instance() {
    static MapDetector instance;
    return instance;
}

MapDetector::MapDetector() {
    // 升格之恋
    m_rules["升格之恋"] = {
        9,
        {
            { 6, {"Ji'nara", "Ji’nara", "Джи-нара", "吉娜拉", "지나라"} },
            { 8, {
                "Elemental de Slayn", "Elementale di Slayn", "Slayn Elemental", "Slayn-Elementar", "Élémentaire de Slayn", "Żywiołak ze Slayn",
                "Элементаль Слейна", "史雷因元素獸", "斯雷恩元素生物", "슬레인 원시 생물"
            }}
        }
    };

    // 死亡摇篮
    m_rules["死亡摇篮"] = {
        6,
        {
            { 4, {
                "Amons Spezialkräfte", "Forze speciali di Amon", "Forças Especiais de Amon", "Fuerzas de Amón especiales",
                "Fuerzas especiales de Amon", "Siły specjalne Amona", "Special Amon's Forces", "Troupes spéciales d’Amon", "Спецвойска Амуна",
                "亞蒙的特殊軍隊", "埃蒙的特战队", "특수 아몬의 병력"
            }},
            { 5, {
                "Amons Spezialkräfte", "Forze speciali di Amon", "Forças Especiais de Amon", "Fuerzas de Amón especiales",
                "Fuerzas especiales de Amon", "Siły specjalne Amona", "Special Amon's Forces", "Troupes spéciales d’Amon", "Спецвойска Амуна",
                "亞蒙的特殊軍隊", "埃蒙的特战队", "특수 아몬의 병력"
            }}
        }
    };

    // 亡者之夜
    m_rules["亡者之夜"] = {
        7,
        {
            { 4, {"Contaminé", "Infestado", "Infestados", "Infestati", "Infested", "Verseuchte", "Zainfekowani", "Зараженные", "受到感染", "感染体", "감염"} },
            { 5, {
                "Sensor Tower", "Sensorturm", "Torre Sensorial", "Torre de sensores", "Torre di rilevamento", "Tour de détection", "Wieża radarowa",
                "Радарная вышка", "感应塔", "感應塔", "감지탑"
            }}
        }
    };

    // 天界封锁
    m_rules["天界封锁"] = {
        4,
        {
            { 2, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }},
            { 3, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }}
        }
    };

    // 净网行动
    m_rules["净网行动"] = {
        6,
        {
            { 2, {
                "Hologram czyścicieli", "Holograma de los Purificadores", "Holograma de purificador", "Holograma do Purificador",
                "Hologramme de Purificateur", "Ologramma dei Purificatori", "Purifier Hologram", "Richter-Hologramm", "Голограмма", "净化者全息投影",
                "淨化者全像部隊", "정화자 홀로그램"
            }},
            { 4, {"Megalit", "Megalite", "Megalith", "Megalito", "Mégalithe", "Мегалит", "碩像儀", "麦加利斯", "메가리스"} }
        }
    };

    // 营救矿工
    m_rules["营救矿工"] = {
        11,
        {
            { 7, {
                "Górnicy kel-moriańscy", "Kel-Morian Miners", "Kel-morianische Kolonie", "Minatori kelmoriani", "Mineiros Kel-Morianos",
                "Mineros de Kel-Moria", "Mineros kelmorianos", "Mineurs kel-morians", "Келморийские шахтеры", "凯莫瑞安矿工", "凱爾莫瑞亞礦工", "켈모리안 광부"
            }},
            { 9, {
                "Górnicy kel-moriańscy", "Kel-Morian Miners", "Kel-morianische Kolonie", "Minatori kelmoriani", "Mineiros Kel-Morianos",
                "Mineros de Kel-Moria", "Mineros kelmorianos", "Mineurs kel-morians", "Келморийские шахтеры", "凯莫瑞安矿工", "凱爾莫瑞亞礦工", "켈모리안 광부"
            }}
        }
    };

    // 机会渺茫
    m_rules["机会渺茫"] = {
        6,
        {
            { 4, {"Terracino", "Terrazine", "Terrazingas", "Terrazino", "Terrazyt", "Терразин", "地嗪", "態化氫", "테라진"} },
            { 5, {"Egon Stetmann", "Игон Стетманн", "伊崗‧斯特曼", "艾贡·斯台特曼", "이곤 스텟먼"} }
        }
    };

    // 湮灭快车
    m_rules["湮灭快车"] = {
        7,
        {
            { 5, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }},
            { 6, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }}
        }
    };

    // 聚铁成兵
    m_rules["聚铁成兵"] = {
        6,
        {
            { 4, {"Balio", "Balius", "«Балий»", "巴利俄斯", "巴流斯", "발리우스"} },
            { 5, {
                "Moebius Train", "Moebius-Zug", "Pociąg Gwardii Moebiusa", "Train de Möbius", "Trem da Moebius", "Tren Moebius", "Tren de Moebius",
                "Treno Moebius", "Поезд корпуса Мебиуса", "莫比斯列車", "莫比斯列车", "뫼비우스 열차"
            }}
        }
    };

    // 克哈裂痕
    m_rules["克哈裂痕"] = {
        6,
        {
            { 4, {
                "Esquirla del Vacío", "Fragment du Vide", "Fragmento del vacío", "Fragmento do Vazio", "Frammento del Vuoto", "Leerensplitter",
                "Odłamek otchłani", "Void Shard", "Осколок Пустоты", "虚空碎片", "虛空晶體", "공허의 파편"
            }},
            { 5, {"Piraci", "Piratas", "Piraten", "Pirates", "Pirati", "Пираты", "海盗", "海盜", "해적"} }
        }
    };

    // 黑暗杀星
    m_rules["黑暗杀星"] = {
        8,
        {
            { 6, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }},
            { 7, {"Evacuados", "Evacuees", "Evakuierte", "Ewakuanci", "Rescapés", "Sfollati", "Беженцы", "待撤離人員", "待救者", "피난민"} }
        }
    };

    // 往日神庙-A
    m_rules["往日神庙-A"] = {
        9,
        {
            { 6, {"Tempel", "Tempio", "Temple", "Templo", "Świątynia", "Храм", "神庙", "神殿", "사원"} },
            { 8, {"Felsen", "Rocas", "Rocce", "Rochas", "Rochers", "Rocks", "Skały", "Камни", "岩石", "바위"} }
        }
    };
    m_rules["往日神庙-B"] = m_rules["往日神庙-A"];

    // 熔火危机
    m_rules["熔火危机"] = {
        8,
        {
            { 6, {
                "Magmasalamander", "Magmowa salamandra", "Molten Salamander", "Salamandra Incandescente", "Salamandra de fuego", "Salamandra lavica",
                "Salamandra ígnea", "Salamandre magmatique", "Огненная саламандра", "熔岩巨蜥", "熔岩蜥蜴", "용암 도롱뇽"
            }},
            { 7, {"Civiles", "Civili", "Civilians", "Civils", "Civis", "Cywile", "Zivilisten", "Мирные жители", "平民", "민간인"} }
        }
    };

    // 虚空降临
    m_rules["虚空降临"] = {
        7,
        {
            { 5, {
                "Canal de transfert", "Conducto de distorsión", "Conducto de transposición", "Conduíte de Dobra", "Tunel przesyłowy",
                "Tunnel dimensionale", "Warp Conduit", "Warpverbindung", "Канал искривления", "时空航道", "躍傳中繼站", "차원로"
            }},
            { 6, {
                "Equipe de Pesquisa Científica", "Equipo de investigación científica", "Forschungsteam", "Scientific Research Team",
                "Squadra di ricerca scientifica", "Zespół badawczy", "Équipe de recherche scientifique", "Команда исследователей", "科學研發隊", "科研队伍",
                "과학 연구 팀"
            }}
        }
    };

    // 虚空撕裂-左
    m_rules["虚空撕裂-左"] = {
        6,
        {
            { 4, {
                "Amon's Forces", "Amons Streitkräfte", "Forze di Amon", "Forças de Amon", "Fuerzas de Amon", "Fuerzas de Amón", "Troupes d’Amon",
                "Wojska Amona", "Войска Амуна", "亞蒙的軍隊", "埃蒙的部队", "아몬의 병력"
            }},
            { 5, {
                "Forze di Sgt. Hammer", "Forças da Sgto. Marreta", "Fuerzas de la Sargento Maza", "Fuerzas de la Sgto. Martillo",
                "Oddziały sierż. Petardy", "Sergeant Hammers Streitkräfte", "Sgt. Hammer's Forces", "Troupes du sgt Marteau", "Силы сержанта Кувалды",
                "榔頭中士的部隊", "重锤军士的部队", "해머 상사의 병력"
            }}
        }
    };
    m_rules["虚空撕裂-右"] = m_rules["虚空撕裂-左"];
}

std::string MapDetector::Identify(const std::vector<PlayerInfo>& players, const std::string& map_hint) {
    int length = (int)players.size();
    std::vector<std::string> candidates;
    for (const auto& kv : m_rules) {
        candidates.push_back(kv.first);
    }
    if (!map_hint.empty() && m_rules.find(map_hint) != m_rules.end()) {
        auto it = std::find(candidates.begin(), candidates.end(), map_hint);
        if (it != candidates.end()) {
            candidates.erase(it);
            candidates.insert(candidates.begin(), map_hint);
        }
    }

    for (const auto& map_name : candidates) {
        const auto& rule = m_rules[map_name];
        if (rule.total_players != length) {
            continue;
        }

        bool match = true;
        for (const auto& check_pair : rule.checks) {
            int slot = check_pair.first;
            if (slot < 0 || slot >= length) {
                match = false;
                break;
            }
            const std::string& actual_name = players[slot].name;
            if (check_pair.second.find(actual_name) == check_pair.second.end()) {
                match = false;
                break;
            }
        }
        if (match) {
            return map_name;
        }
    }
    return "";
}
