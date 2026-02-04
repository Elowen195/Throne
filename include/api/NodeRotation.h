#pragma once

#include <QString>
#include <QMutex>
#include <QHash>
#include <memory>
#include <functional>

#include "include/api/HttpAPI.h"

namespace Configs {
    class Group;
    class Profile;
}

namespace API {

    class NodeRotationManager {
    public:
        NodeRotationManager();
        ~NodeRotationManager();

        // Switch to next node in current group
        NodeRotationResult SwitchToNextInCurrentGroup();

        // Get current node index for a group
        int GetCurrentNodeIndex(int groupId);

        // Set callback for starting a profile
        void SetStartProfileCallback(std::function<void(int)> callback);

    private:
        // Get current group
        std::shared_ptr<Configs::Group> GetCurrentGroup() const;

        // Get profile by ID
        std::shared_ptr<Configs::Profile> GetProfile(int profileId) const;

        // Store current index for each group
        QHash<int, int> group_current_index_;
        QMutex mutex_;

        std::function<void(int)> start_profile_callback_;
    };

    // Global instance
    inline NodeRotationManager *nodeRotationManager = nullptr;

} // namespace API
