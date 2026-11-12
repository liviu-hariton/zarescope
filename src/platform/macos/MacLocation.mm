// SPDX-License-Identifier: GPL-3.0-or-later
//
// LocationFinder on macOS: CLLocationManager, created on the main thread so
// its delegate is called from the main run loop, which GLFW's event polling
// runs every frame.
//
// Flow: Location Services off → Disabled. Permission refused or restricted →
// Denied. Not asked yet → requestWhenInUseAuthorization, then continue when
// the user answers. Allowed → requestLocation (one fix). A 30-s timer ends
// whatever is still pending.
//
// Info.plist carries NSLocationUsageDescription and
// NSLocationWhenInUseUsageDescription (the text in macOS's prompt), and the
// packaged app the hardened-runtime entitlement
// com.apple.security.personal-information.location (docs/RELEASE.md).

#include "platform/Location.h"

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

using zarescope::platform::LocationFinder;
using zarescope::platform::LocationResult;

struct LocationFinder::Impl {
    CLLocationManager* manager = nil;
    id<CLLocationManagerDelegate> delegate = nil;
    std::function<void(const LocationResult&)> done;
    uint64_t generation = 0;  // tells a stale timeout from the current request

    void finish(const LocationResult& result) {
        if (!done) {
            return;  // already answered: exactly once
        }
        auto callback = std::move(done);
        done = nullptr;
        ++generation;
        callback(result);
    }

    static LocationResult error(LocationResult::Error kind, std::string detail = {}) {
        LocationResult result;
        result.error = kind;
        result.detail = std::move(detail);
        return result;
    }

    // Called when permission is known: ask for one fix, or give up.
    void proceed() {
        if (!done) {
            return;
        }
        switch (manager.authorizationStatus) {
            case kCLAuthorizationStatusNotDetermined:
                return;  // still waiting for the user's answer
            case kCLAuthorizationStatusDenied:
            case kCLAuthorizationStatusRestricted:
                finish(error(LocationResult::Error::Denied));
                return;
            default:
                [manager requestLocation];
                return;
        }
    }
};

@interface ZarescopeLocationDelegate : NSObject <CLLocationManagerDelegate>
@property(nonatomic, assign) LocationFinder::Impl* owner;
@end

@implementation ZarescopeLocationDelegate

- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)manager {
    (void)manager;
    if (self.owner != nullptr) {
        self.owner->proceed();
    }
}

- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray<CLLocation*>*)locations {
    (void)manager;
    CLLocation* location = locations.lastObject;
    if (self.owner == nullptr || location == nil) {
        return;
    }
    LocationResult result;
    result.error = LocationResult::Error::None;
    result.latDeg = location.coordinate.latitude;
    result.lonDeg = location.coordinate.longitude;
    result.accuracyM = location.horizontalAccuracy;
    if (location.verticalAccuracy >= 0.0) {
        result.altitudeM = location.altitude;
    }
    self.owner->finish(result);
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
    (void)manager;
    if (self.owner == nullptr) {
        return;
    }
    if ([error.domain isEqualToString:kCLErrorDomain] && error.code == kCLErrorDenied) {
        self.owner->finish(LocationFinder::Impl::error(LocationResult::Error::Denied));
        return;
    }
    const char* text = error.localizedDescription.UTF8String;
    self.owner->finish(LocationFinder::Impl::error(LocationResult::Error::Failed,
                                                   text != nullptr ? text : "unknown error"));
}

@end

namespace zarescope::platform {

LocationFinder::LocationFinder() : m_impl(std::make_unique<Impl>()) {}

LocationFinder::~LocationFinder() {
    m_impl->finish(Impl::error(LocationResult::Error::Failed, "cancelled"));
    if (m_impl->manager != nil) {
        m_impl->manager.delegate = nil;
    }
    if (m_impl->delegate != nil) {
        static_cast<ZarescopeLocationDelegate*>(m_impl->delegate).owner = nullptr;
    }
}

void LocationFinder::request(std::function<void(const LocationResult&)> done) {
    if (m_impl->done) {
        done(Impl::error(LocationResult::Error::Failed, "busy"));
        return;
    }
    if (![CLLocationManager locationServicesEnabled]) {
        done(Impl::error(LocationResult::Error::Disabled));
        return;
    }
    if (m_impl->manager == nil) {
        ZarescopeLocationDelegate* delegate = [[ZarescopeLocationDelegate alloc] init];
        delegate.owner = m_impl.get();
        m_impl->delegate = delegate;
        m_impl->manager = [[CLLocationManager alloc] init];
        m_impl->manager.delegate = delegate;
        m_impl->manager.desiredAccuracy = kCLLocationAccuracyHundredMeters;
    }
    m_impl->done = std::move(done);

    // Give up after 30 s, unless this request has been answered by then.
    const uint64_t generation = m_impl->generation;
    Impl* impl = m_impl.get();
    __weak id<CLLocationManagerDelegate> alive = m_impl->delegate;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
      if (alive != nil && impl->generation == generation) {
          impl->finish(Impl::error(LocationResult::Error::Failed, "no fix within 30 s"));
      }
    });

    if (m_impl->manager.authorizationStatus == kCLAuthorizationStatusNotDetermined) {
        [m_impl->manager requestWhenInUseAuthorization];  // proceed() runs on the answer
    } else {
        m_impl->proceed();
    }
}

}  // namespace zarescope::platform
