#include <ui/ui_SettingsMenuLayout.hpp>
#include <os/os_Account.hpp>
#include <os/os_Misc.hpp>
#include <util/util_Convert.hpp>
#include <ui/ui_QMenuApplication.hpp>
#include <fs/fs_Stdio.hpp>
#include <net/net_Service.hpp>
#include <am/am_LibraryApplet.hpp>

extern ui::QMenuApplication::Ref qapp;
extern cfg::ProcessedTheme theme;
extern cfg::Config config;

namespace ui
{
    template<typename T>
    std::string EncodeForSettings(T t)
    {
        return cfg::GetLanguageString(config.main_lang, config.default_lang, "set_unknown_value");
    }

    template<>
    std::string EncodeForSettings<std::string>(std::string t)
    {
        return "\"" + t + "\"";
    }
    
    template<>
    std::string EncodeForSettings<u32>(u32 t)
    {
        return "\"" + std::to_string(t) + "\"";
    }

    template<>
    std::string EncodeForSettings<bool>(bool t)
    {
        return t ? cfg::GetLanguageString(config.main_lang, config.default_lang, "set_true_value") : cfg::GetLanguageString(config.main_lang, config.default_lang, "set_false_value");
    }

    SettingsMenuLayout::SettingsMenuLayout()
    {
        this->SetBackgroundImage(cfg::ProcessedThemeResource(theme, "ui/Background.png"));

        pu::ui::Color textclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("text_color", "#e1e1e1ff"));
        pu::ui::Color menufocusclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("menu_focus_color", "#5ebcffff"));
        pu::ui::Color menubgclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("menu_bg_color", "#0094ffff"));

        this->infoText = pu::ui::elm::TextBlock::New(0, 100, cfg::GetLanguageString(config.main_lang, config.default_lang, "set_info_text"));
        this->infoText->SetColor(textclr);
        this->infoText->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        qapp->ApplyConfigForElement("settings_menu", "info_text", this->infoText);
        this->Add(this->infoText);

        this->settingsMenu = pu::ui::elm::Menu::New(200, 160, 880, menubgclr, 100, 4);
        this->settingsMenu->SetOnFocusColor(menufocusclr);
        qapp->ApplyConfigForElement("settings_menu", "settings_menu_item", this->settingsMenu);
        this->Add(this->settingsMenu);

        this->SetOnInput(std::bind(&SettingsMenuLayout::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    }

    void SettingsMenuLayout::Reload()
    {
        this->settingsMenu->ClearItems();
        this->settingsMenu->SetSelectedIndex(0);
        
        char consolename[SET_MAX_NICKNAME_SIZE] = {};
        setsysGetDeviceNickname(consolename);
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_console_nickname"), EncodeForSettings<std::string>(consolename), 0);
        TimeLocationName loc = {};
        timeGetDeviceLocationName(&loc);
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_console_timezone"), EncodeForSettings<std::string>(loc.name), -1);
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_viewer_enabled"), EncodeForSettings(config.viewer_usb_enabled), 1);
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_flog_enabled"), EncodeForSettings(config.system_title_override_enabled), 2);
        std::string connectednet = cfg::GetLanguageString(config.main_lang, config.default_lang, "set_wifi_none");
        if(net::HasConnection())
        {
            net::NetworkProfileData data = {};
            net::GetCurrentNetworkProfile(&data);
            connectednet = data.wifi_name;
        }
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_wifi_name"), EncodeForSettings(connectednet), 3);

        u64 lcode = 0;
        s32 ilang = 0;
        setGetLanguageCode(&lcode);
        setMakeLanguage(lcode, &ilang);
        this->PushSettingItem(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_console_lang"), EncodeForSettings(os::LanguageNames[ilang]), 4);
    }

    void SettingsMenuLayout::PushSettingItem(std::string name, std::string value_display, int id)
    {
        pu::ui::Color textclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("text_color", "#e1e1e1ff"));
        auto itm = pu::ui::elm::MenuItem::New(name + ": " + value_display);
        itm->AddOnClick(std::bind(&SettingsMenuLayout::setting_Click, this, id));
        itm->SetIcon(cfg::ProcessedThemeResource(theme, "ui/Setting" + std::string((id < 0) ? "No" : "") + "Editable.png"));
        itm->SetColor(textclr);
        this->settingsMenu->AddItem(itm);
    }

    void SettingsMenuLayout::setting_Click(u32 id)
    {
        bool reload_need = false;
        switch(id)
        {
            case 0:
            {
                SwkbdConfig swkbd;
                swkbdCreate(&swkbd, 0);
                swkbdConfigSetGuideText(&swkbd, cfg::GetLanguageString(config.main_lang, config.default_lang, "swkbd_console_nick_guide").c_str());
                char consolename[SET_MAX_NICKNAME_SIZE] = {};
                setsysGetDeviceNickname(consolename);
                swkbdConfigSetInitialText(&swkbd, consolename);
                swkbdConfigSetStringLenMax(&swkbd, 32);
                char name[SET_MAX_NICKNAME_SIZE] = {0};
                auto rc = swkbdShow(&swkbd, name, SET_MAX_NICKNAME_SIZE);
                swkbdClose(&swkbd);
                if(R_SUCCEEDED(rc))
                {
                    setsysSetDeviceNickname(name);
                    reload_need = true;
                }
                break;
            }
            case 1:
            {
                auto sopt = qapp->CreateShowDialog(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_viewer_enabled"), cfg::GetLanguageString(config.main_lang, config.default_lang, "set_viewer_info") + "\n" + (config.viewer_usb_enabled ? cfg::GetLanguageString(config.main_lang, config.default_lang, "set_disable_conf") : cfg::GetLanguageString(config.main_lang, config.default_lang, "set_enable_conf")), { cfg::GetLanguageString(config.main_lang, config.default_lang, "yes"), cfg::GetLanguageString(config.main_lang, config.default_lang, "cancel") }, true);
                if(sopt == 0)
                {
                    config.viewer_usb_enabled = !config.viewer_usb_enabled;
                    reload_need = true;
                    qapp->ShowNotification(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_changed_reboot"));
                }
                break;
            }
            case 2:
            {
                auto sopt = qapp->CreateShowDialog(cfg::GetLanguageString(config.main_lang, config.default_lang, "set_flog_enabled"), cfg::GetLanguageString(config.main_lang, config.default_lang, "set_flog_info") + "\n" + (config.viewer_usb_enabled ? cfg::GetLanguageString(config.main_lang, config.default_lang, "set_disable_conf") : cfg::GetLanguageString(config.main_lang, config.default_lang, "set_enable_conf")), { cfg::GetLanguageString(config.main_lang, config.default_lang, "yes"), cfg::GetLanguageString(config.main_lang, config.default_lang, "cancel") }, true);
                if(sopt == 0)
                {
                    config.system_title_override_enabled = !config.system_title_override_enabled;
                    reload_need = true;
                }
                break;
            }
            case 3:
            {
                u8 in[28] = {0};
                *(u32*)in = 1; // 0 = normal, 1 = qlaunch, 2 = starter?
                u8 out[8] = {0};

                am::LibraryAppletQMenuLaunchAnd(AppletId_netConnect, 0, in, sizeof(in), out, sizeof(out), [&]() -> bool
                {
                    return !am::QMenuIsHomePressed();
                });
                auto rc = *(u32*)in;

                // Apparently 0 is returned when user connects to/selects a different WiFi network
                if(R_SUCCEEDED(rc)) reload_need = true;
                break;
            }
            case 4:
            {
                qapp->FadeOut();
                qapp->LoadSettingsLanguagesMenu();
                qapp->FadeIn();

                break;
            }
        }
        if(reload_need)
        {
            cfg::SaveConfig(config);
            this->Reload();
        }
    }

    void SettingsMenuLayout::OnInput(u64 down, u64 up, u64 held, pu::ui::Touch pos)
    {
        bool ret = am::QMenuIsHomePressed();
        if(down & KEY_B) ret = true;
        if(ret)
        {
            qapp->FadeOut();
            qapp->LoadMenu();
            qapp->FadeIn();
        }
    }
}