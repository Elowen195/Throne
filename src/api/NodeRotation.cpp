#include "include/api/NodeRotation.h"

#include "include/database/entities/Group.h"
#include "include/database/entities/Profile.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/global/Configs.hpp"

namespace API {

    NodeRotationManager::NodeRotationManager() = default;

    NodeRotationManager::~NodeRotationManager() = default;

    void NodeRotationManager::SetStartProfileCallback(std::function<void(int)> callback) {
        start_profile_callback_ = std::move(callback);
    }

    NodeRotationResult NodeRotationManager::SwitchToNextInCurrentGroup() {
        QMutexLocker locker(&mutex_);

        NodeRotationResult result;
        result.success = false;

        auto group = GetCurrentGroup();
        if (!group) {
            result.error = "Current group not found";
            return result;
        }

        result.group_name = group->name;

        auto profiles = group->Profiles();
        if (profiles.isEmpty()) {
            result.error = QString("Group '%1' has no profiles").arg(group->name);
            return result;
        }

        result.total_nodes = profiles.size();

        int currentIndex = group_current_index_.value(group->id, -1);
        if (currentIndex < 0 || currentIndex >= profiles.size()) {
            int startedId = -1;
            if (Configs::dataManager && Configs::dataManager->settingsRepo) {
                startedId = Configs::dataManager->settingsRepo->started_id;
            }
            if (startedId >= 0) {
                int idx = profiles.indexOf(startedId);
                if (idx >= 0) {
                    currentIndex = idx;
                }
            }
        }

        if (currentIndex >= 0 && currentIndex < profiles.size()) {
            int previousProfileId = profiles[currentIndex];
            auto previousProfile = GetProfile(previousProfileId);
            if (previousProfile) {
                result.previous_node_id = previousProfileId;
                result.previous_node_name = previousProfile->outbound->DisplayTypeAndName();
                result.previous_node_index = currentIndex;
            }
        }

        int nextIndex = (currentIndex + 1) % profiles.size();
        group_current_index_[group->id] = nextIndex;

        int nextProfileId = profiles[nextIndex];
        auto nextProfile = GetProfile(nextProfileId);
        if (!nextProfile) {
            result.error = QString("Profile with ID %1 not found").arg(nextProfileId);
            return result;
        }

        result.current_node_id = nextProfileId;
        result.current_node_name = nextProfile->outbound->DisplayTypeAndName();
        result.current_node_index = nextIndex;

        if (start_profile_callback_) {
            start_profile_callback_(nextProfileId);
        } else {
            result.error = "Start profile callback not set";
            return result;
        }

        result.success = true;
        return result;
    }

    int NodeRotationManager::GetCurrentNodeIndex(int groupId) {
        QMutexLocker locker(&mutex_);
        return group_current_index_.value(groupId, -1);
    }

    std::shared_ptr<Configs::Group> NodeRotationManager::GetCurrentGroup() const {
        if (!Configs::dataManager || !Configs::dataManager->groupsRepo) {
            return nullptr;
        }
        return Configs::dataManager->groupsRepo->CurrentGroup();
    }

    std::shared_ptr<Configs::Profile> NodeRotationManager::GetProfile(int profileId) const {
        if (!Configs::dataManager || !Configs::dataManager->profilesRepo) {
            return nullptr;
        }
        return Configs::dataManager->profilesRepo->GetProfile(profileId);
    }

} // namespace API
