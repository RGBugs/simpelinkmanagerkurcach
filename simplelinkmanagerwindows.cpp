// Защита от min/max макросов Windows (обязательно до windows.h)
#define NOMINMAX

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <limits>
#include <windows.h>   // SetConsoleOutputCP
#include <io.h>
#include <fcntl.h>

namespace fs = std::filesystem;

class SymLinkManager {
public:
    explicit SymLinkManager(const fs::path& root) : root_(fs::absolute(root)) {
        if (!fs::exists(root_)) {
            throw std::runtime_error("Корневая директория не существует: " + root_.string());
        }
        if (!fs::is_directory(root_)) {
            throw std::runtime_error("Указанный путь не является директорией: " + root_.string());
        }
    }

    std::vector<fs::path> findAllSymLinks() const {
        std::vector<fs::path> links;
        for (const auto& entry : fs::recursive_directory_iterator(root_)) {
            if (fs::is_symlink(entry)) {
                links.push_back(entry.path());
            }
        }
        return links;
    }

    std::vector<fs::path> findBrokenSymLinks() const {
        std::vector<fs::path> broken;
        for (const auto& link : findAllSymLinks()) {
            if (!fs::exists(fs::read_symlink(link))) {
                broken.push_back(link);
            }
        }
        return broken;
    }

    void createSymLink(const fs::path& linkPath, const fs::path& target) {
        fs::path absLink = root_ / linkPath;
        fs::path absTarget = target.is_absolute() ? target : fs::absolute(target);
        if (auto parent = absLink.parent_path(); !fs::exists(parent)) {
            fs::create_directories(parent);
        }
        fs::create_symlink(absTarget, absLink);
        std::wcout << L"Создана ссылка: " << absLink.wstring() << L" -> " << absTarget.wstring() << std::endl;
    }

    void removeSymLink(const fs::path& linkPath) {
        fs::path absLink = root_ / linkPath;
        if (fs::is_symlink(absLink)) {
            fs::remove(absLink);
            std::wcout << L"Удалена ссылка: " << absLink.wstring() << std::endl;
        }
        else {
            throw std::runtime_error("Не является символьной ссылкой: " + absLink.string());
        }
    }

    void convertAbsoluteToRelative() {
        auto links = findAllSymLinks();
        for (const auto& link : links) {
            fs::path target = fs::read_symlink(link);
            if (target.is_absolute()) {
                fs::path relativeTarget = fs::relative(target, link.parent_path());
                fs::remove(link);
                fs::create_symlink(relativeTarget, link);
                std::wcout << L"Преобразован: " << link.wstring()
                    << L"\n\tбыло: " << target.wstring()
                    << L"\n\tстало: " << relativeTarget.wstring() << std::endl;
            }
        }
    }

    void displayAllLinks() const {
        auto links = findAllSymLinks();
        if (links.empty()) {
            std::wcout << L"Символические ссылки не найдены.\n";
            return;
        }
        std::wcout << L"Найдено симлинков: " << links.size() << L"\n";
        for (const auto& link : links) {
            std::wcout << L"  " << link.wstring() << L" -> " << fs::read_symlink(link).wstring() << std::endl;
        }
    }

private:
    fs::path root_;
};

void printMenu() {
    std::wcout << L"\n=== Меню управления символьными ссылками ===\n";
    std::wcout << L"1. Показать все симлинки в проекте\n";
    std::wcout << L"2. Показать только битые симлинки\n";
    std::wcout << L"3. Создать новый симлинк\n";
    std::wcout << L"4. Удалить симлинк\n";
    std::wcout << L"5. Преобразовать абсолютные симлинки в относительные\n";
    std::wcout << L"0. Выход\n";
    std::wcout << L"Выберите действие: ";
}

std::wstring inputPath(const std::wstring& prompt) {
    std::wcout << prompt;
    std::wstring path;
    std::getline(std::wcin, path);
    return path;
}

int main(int argc, char* argv[]) {
    // Настройка консоли на UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Перевод потоков на UTF-8 для wcout/wcin
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);

    try {
        fs::path rootDir = fs::current_path();
        if (argc > 1) {
            rootDir = argv[1];
        }
        std::wcout << L"Рабочая директория проекта: " << rootDir.wstring() << std::endl;

        SymLinkManager manager(rootDir);

        int choice = -1;
        while (choice != 0) {
            printMenu();
            std::wcin >> choice;
            std::wcin.ignore((std::numeric_limits<std::streamsize>::max)(), L'\n');

            switch (choice) {
            case 1:
                manager.displayAllLinks();
                break;
            case 2: {
                auto broken = manager.findBrokenSymLinks();
                if (broken.empty()) {
                    std::wcout << L"Битых симлинков не найдено.\n";
                }
                else {
                    std::wcout << L"Битые симлинки (" << broken.size() << L"):\n";
                    for (const auto& link : broken) {
                        std::wcout << L"  " << link.wstring() << L" -> " << fs::read_symlink(link).wstring() << std::endl;
                    }
                }
                break;
            }
            case 3: {
                std::wstring linkPath = inputPath(L"Введите путь для новой ссылки (относительно корня проекта): ");
                std::wstring target = inputPath(L"Введите цель ссылки (абсолютный или относительный путь): ");
                manager.createSymLink(linkPath, target);
                break;
            }
            case 4: {
                std::wstring linkPath = inputPath(L"Введите путь к удаляемой ссылке (относительно корня): ");
                manager.removeSymLink(linkPath);
                break;
            }
            case 5:
                manager.convertAbsoluteToRelative();
                break;
            case 0:
                std::wcout << L"Выход.\n";
                break;
            default:
                std::wcout << L"Неверный выбор. Попробуйте снова.\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::wcerr << L"Ошибка: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
