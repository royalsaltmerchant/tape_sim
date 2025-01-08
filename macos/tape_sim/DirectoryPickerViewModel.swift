import AppKit
import SwiftUI

class DirectoryPickerViewModel: ObservableObject {
    func promptUserForDirectory() {
        var userDidSelectDirectory = false
        
        while !userDidSelectDirectory {
            let panel = NSOpenPanel()
            panel.canChooseDirectories = true
            panel.canCreateDirectories = true
            panel.canChooseFiles = false
            panel.allowsMultipleSelection = false
            panel.message = "Please select the working directory for your audio files."
            
            if panel.runModal() == .OK, let selectedURL = panel.url {
                if selectedURL.startAccessingSecurityScopedResource() {
                    let selectedPath = selectedURL.path
                    print("Selected working directory: \(selectedPath)")
                    
                    // Convert the path to a C string and pass it to the C function
                    let cPath = strdup(selectedPath)
                    defer { free(cPath) }  // Ensure to free the allocated memory
                    
                    if let cPath = cPath {
                        onSetAppDirPath(cPath)
                    }
                    
                    userDidSelectDirectory = true  // Mark as successful
                } else {
                    print("Failed to access security-scoped resource.")
                }
            } else {
				NSApp.terminate(nil)
            }
        }
    }
}

