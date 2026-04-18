#include "MockCloudProvider.h"

std::vector<FileItem> MockCloudProvider::list_files(const std::string& path) {
    if (path == "/") {
        return {
            {
                "1",
                "Documents",
                "/Documents",
                true,
                0,
                "inode/directory",
                "2026-04-17 10:15",
                false
            },
            {
                "2",
                "Report.pdf",
                "/Report.pdf",
                false,
                245760,
                "application/pdf",
                "2026-04-17 10:20",
                true
            },
            {
                "3",
                "Photo.png",
                "/Photo.png",
                false,
                1048576,
                "image/png",
                "2026-04-17 10:25",
                true
            },
            {
                "4",
                "Archive.zip",
                "/Archive.zip",
                false,
                5242880,
                "application/zip",
                "2026-04-17 10:30",
                false
            }
        };
    }

    if (path == "/Documents") {
        return {
            {
                "5",
                "Notes.txt",
                "/Documents/Notes.txt",
                false,
                4096,
                "text/plain",
                "2026-04-17 11:00",
                true
            },
            {
                "6",
                "Presentation.pptx",
                "/Documents/Presentation.pptx",
                false,
                2097152,
                "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                "2026-04-17 11:05",
                false
            },
            {
                "7",
                "Contracts",
                "/Documents/Contracts",
                true,
                0,
                "inode/directory",
                "2026-04-17 11:10",
                false
            }
        };
    }

    if (path == "/Documents/Contracts") {
        return {
            {
                "8",
                "Agreement.docx",
                "/Documents/Contracts/Agreement.docx",
                false,
                153600,
                "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                "2026-04-17 11:20",
                true
            },
            {
                "9",
                "Invoice.pdf",
                "/Documents/Contracts/Invoice.pdf",
                false,
                86016,
                "application/pdf",
                "2026-04-17 11:25",
                false
            }
        };
    }

    return {};
}